#include "server.h"
#include "list.h"

int compare_ppd(ppd_t *p0, ppd_t *p1) {
  return strcmp(p0->uri, p1->uri);
}

void initialize() {
  if (getenv("SNAP")) {
    snap = strdup(getenv("SNAP"));
  } else
    snap = strdup("");

  char *p = getenv("SNAP_COMMON");
  if (p) {
    tmpdir = calloc(strlen(p) + 5, sizeof(char));
    snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", p);
  } else {
    p = getenv("TMPDIR");
    if (p)
      tmpdir = strdup(p);
    else
      tmpdir = strdup("/tmp");
  }

  con_devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);
  temp_devices = cupsArrayNew((cups_array_func_t)compare_devices, NULL);
  ppd_list = cupsArrayNew((cups_array_func_t)compare_ppd, NULL);
}

int parsePpdLine(char *line) {
  ppd_t* newppd;
  int i = 0, j = 0;

  if ((newppd = calloc(1, sizeof(ppd_t))) == NULL) {
    fprintf (stderr, "ERROR: Unable to allocate memory\n");
    return -1;
  }
  for (i = 0; i < sizeof(newppd->uri) - 1 && line[i]; i++) {
    if (line[i] == ' ')
      break;
    newppd->uri[i] = line[i];
  }
  newppd->uri[i] = 0;
  while (line[i] != '(' && line[i])
    i++;
  i++;
  for (j = 0; j < sizeof(newppd->name) - 1 && line[i]; j++, i++) {
    if (line[i] == ')')
      break;
    newppd->name[j] = line[i];
  }
  newppd->name[j] = 0;
  int res = cupsArrayAdd(ppd_list, newppd);
  return res;
}

int deviceList() {
  const char  *serverbin;        /* ServerBin */
  char        program[2048];     /* Full Path to program */
  char        *argv[7];
  char        *env[7];
  char        name[32], reques_id[16], limit[16],
              timeout[16], user_id[16], options[1024];
  char        serverdir[1024], serverroot[1024], datadir[1024];
  process_t   *process;
  int         process_pid, status;
  char        includes[4096];
  cups_file_t *errlog;
  char        *p;

  if ((process = calloc(1, sizeof(process_t))) == NULL) {
    debug_printf("ERROR: Ran Out of Memory!\n");
    return (-1);
  }

  cupsArrayClear(temp_devices);
  cupsArrayClear(con_devices);

  strcpy(includes,"-");
  strcpy(reques_id, DEVICED_REQ);
  strcpy(limit, DEVICED_LIM);
  strcpy(timeout, DEVICED_TIM);
  strcpy(user_id, DEVICED_USE);
  strcpy(options, DEVICED_OPT);
  strcpy(name, "deviced");

  p = getenv("BINDIR");
  if (p)
    snprintf(program, sizeof(program), "%s/%s", p, name);
  else
    snprintf(program, sizeof(program), "%s%s/%s", snap, BINDIR, name);
  snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
  snprintf(serverroot, sizeof(serverroot), "%s/etc/cups", snap);
  snprintf(datadir, sizeof(datadir), "%s/usr/share/cups", snap);

  setenv("CUPS_SERVERBIN", serverdir, 1);
  setenv("CUPS_DATADIR", datadir, 1);
  setenv("CUPS_SERVERROOT", serverroot, 1);

  argv[0] = (char*) name;
  argv[1] = (char*) limit;
  argv[2] = (char*) timeout;
  argv[3] = (char*) includes;
  argv[4] = NULL;

  if ((process->pipe = cupsdPipeCommand2(&(process->pid), program, argv,
					 &errlog, 0)) == NULL) {
    debug_printf("ERROR: Unable to execute deviced!\n");
    cupsFileClose(errlog);
    free(process);
    return (-1);
  }
  pthread_t logThread;
  logFromFile2(&logThread, errlog);
  if ((process_pid = waitpid(process->pid, &status, 0)) > 0) {
    while (!parse_line(process));
    pthread_join(logThread, NULL);
  } else {
    fprintf(stdout,
	    "Failed to collect! PID ERROR! %d %s\n",
	    process->pid, strerror(errno));
    return errno;
  }
    
  device_t *dev = cupsArrayFirst(temp_devices);
  for (; dev; dev = cupsArrayNext(temp_devices)) {
    device_t* newDev = deviceCopy(dev);
    cupsArrayAdd(con_devices, newDev);
  }

  return 0;
}

int ppdList() {
  const char *serverbin;
  char program[2048];
  char *argv[6];
  char name[16], operation[8], request_id[4], limit[5], options[1024];
  char ppd_uri[128];
  char ppd_name[1024];  /* Full ppd path */
  char escp_model[256];
  char *envp[6];
  char datadir[1024], serverdir[1024], cachedir[1024];
  char line[4096];
  cups_file_t *errlog;
  process_t *process;
  int  process_pid, status;

  if ((process = calloc(1, sizeof(process_t))) == NULL) {
    debug_printf("ERROR: Ran Out of Memory!\n");
    return (-1);
  }
  strcpy(name, "cups-driverd");
  strcpy(operation, "list");
  strcpy(request_id, "0");
  strcpy(limit, "0");
  strcpy(options, "");
  /*snprintf(options, sizeof(options),
	   "ppd-make-and-model=\'%s\' ppd-device-id=\'%s\'",
	   make_and_model, device_id);*/

  snprintf(datadir, sizeof(datadir), "%s%s", snap, DATADIR);
  snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
  snprintf(cachedir, sizeof(cachedir), "%s", tmpdir);

  setenv("CUPS_DATADIR", datadir, 1);
  setenv("CUPS_SERVERBIN", serverdir, 1);
  setenv("CUPS_CACHEDIR", cachedir, 1);

  snprintf(program, sizeof(program), "%s/daemon/%s", serverdir, name);

  argv[0] = (char*) name;
  argv[1] = (char*) operation;
  argv[2] = (char*) request_id;
  argv[3] = (char*) limit;
  argv[4] = (char*) options;
  argv[5] = NULL;

  debug_printf("DEBUG: Executing cups-driverd at %s\n", program);
  if ((process->pipe = cupsdPipeCommand2(&(process->pid), program,
					 argv, &errlog, 0)) == NULL) {
    debug_printf("ERROR: Unable to execute!\n");
    cupsFileClose(errlog);
    free(process);
    return (-1);
  }
  pthread_t logThread;
  logFromFile2(&logThread, errlog);
  while (cupsFileGets(process->pipe, line, sizeof(line)))
    parsePpdLine(line);
  if ((process_pid = waitpid(process->pid, &status, 0)) > 0)
    pthread_join(logThread, NULL);
  return 0;
}

int printDevices() {
  if (deviceList())
    return 1;

  device_t *dev = cupsArrayFirst(con_devices);
  for (; dev; dev = cupsArrayNext(con_devices))
    printf("\"%s\" \"%s\" \"%s\"\n", dev->device_uri,
	   dev->device_make_and_model, dev->device_id);

  return 0;
}

int printPpdList() {
  if (ppdList())
    return 1;

  ppd_t *p = cupsArrayFirst(ppd_list);
  for (; p; p = cupsArrayNext(ppd_list))
    printf("\"%s\" (%s)\n", p->name, p->uri);

  return 0;
}

int getPPDfile(char* ppd_uri, char** ppdfile) {
  process_t* process;
  char name[16], operation[8];
  char program[PATH_MAX];
  char datadir[1024], serverdir[1024], cachedir[1024];
  cups_file_t *errlog;
  char *argv[6];
  char *filename;
  int namelen = PATH_MAX;
  int status;
  pthread_t logThread;

  if ((filename = calloc(namelen, sizeof(char))) == NULL) {
    fprintf(stderr, "ERROR: Unable to allocate memory.\n");
    return -1;
  }
  
  *ppdfile = filename;

  char ppd_folder[namelen];
  snprintf(ppd_folder, namelen, "%s/ppd/", tmpdir);
  mkdir(ppd_folder, 0777);

  snprintf(filename, namelen, "%s/ppd.XXXXXX", ppd_folder);
  int ppdFD = mkstemp(filename);
  if (ppdFD <= 0)
    return -1;
  close(ppdFD);
  cups_file_t* temp_ppd;
  if ((temp_ppd = cupsFileOpen(filename, "w")) == NULL) {
    fprintf(stderr, "ERROR: Unable to open temporary ppdfile.");
    return -1;
  }

  if ((process = calloc(1, sizeof(process_t))) == NULL) {
    fprintf(stderr, "ERROR: Ran Out of Memory!\n");
    return (-1);
  }

  strcpy(name, "cups-driverd");
  strcpy(operation, "cat");

  snprintf(datadir, sizeof(datadir), "%s%s", snap, DATADIR);
  snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
  snprintf(cachedir, sizeof(cachedir), "%s", tmpdir);

  setenv("CUPS_DATADIR", datadir, 1);
  setenv("CUPS_SERVERBIN", serverdir, 1);
  setenv("CUPS_CACHEDIR", cachedir, 1);

  snprintf(program, sizeof(program), "%s/daemon/%s", serverdir, name);
  
  argv[0] = (char*) name;
  argv[1] = (char*) operation;
  argv[2] = (char*) ppd_uri;
  argv[3] = NULL;
  
  if ((process->pipe = cupsdPipeCommand2(&(process->pid), program,
					 argv, &errlog, 0)) == NULL) {
    debug_printf("ERROR: Unable to execute cups-driverd!\n");
    free(process);
    cupsFileClose(errlog);
    return (-1);
  }

  logFromFile2(&logThread, errlog);
  int counter = print_ppd(process, temp_ppd);
  if ((waitpid(process->pid, &status, 0)) > 0) {
    if(WIFEXITED(status)) {
      int st = 0;
      /*counter = ;*/
      /*while((st = print_ppd(process, tempPPD)) > 0) counter++;*/
    }
    pthread_join(logThread, NULL);
  } else {
    free(process);
    cupsFileClose(temp_ppd);
    return -1;
  }

  cupsFileClose(temp_ppd);
  free(process);
  return 0;
}

void attach(char* device_uri, char* ppd_uri,char* name, int port) {
  char *ppdfile;
  getPPDfile(ppd_uri, &ppdfile);

  char *argv[14];
  char program[PATH_MAX];
  char command[PATH_MAX];
  char port_string[8];
  char datadir[1024], serverdir[1024], cachedir[1024];
  char *p;

  snprintf(program, sizeof(program), "%s%s/ippeveprinter", snap, SBINDIR);
  p = getenv("BINDIR");
  if (p)
    snprintf(command, sizeof(command), "%s/ippprint", p);
  else
    snprintf(command, sizeof(command), "%s%s/ippprint", snap, BINDIR);
  snprintf(port_string, sizeof(port_string), "%d", port);
  argv[0] = (char*) program;
  argv[1] = "-P";
  argv[2] = (char*) ppdfile;
  argv[3] = "-c";
  argv[4] = (char*) command;
  argv[5] = "-p";
  argv[6] = (char*) port_string;
  argv[7] = (char*) name;
  argv[8] = NULL;

  snprintf(datadir, sizeof(datadir), "%s%s", snap, DATADIR);
  snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
  snprintf(cachedir, sizeof(cachedir), "%s", tmpdir);

  setenv("CUPS_DATADIR", datadir, 1);
  setenv("CUPS_SERVERBIN", serverdir, 1);
  setenv("CUPS_CACHEDIR", cachedir, 1);

  setenv("DEVICE_URI", device_uri, 1);
  setenv("PRINTER", name, 1);

  execvp(argv[0], argv);
}

void usage(char *arg) {
  printf("Usage: %s -(p/d)\n"
	 "Usage: %s -D device_uri -P ppd_uri -n port name\n"
	 "-p: Print PPDs\n"
	 "-d: Print Available devices\n", arg, arg);
}

int main(int argc, char *argv[]) {
  initialize();

  int device = 0, ppd = 0;

  if (argc == 2) {
    for (int i = 1; i < argc; i++) {
      if (strlen(argv[i]) == 1) {
	usage(argv[0]);
	return -1;
      }
      if (argv[i][0] != '-') {
	usage(argv[0]);
	return -1;
      }
      switch (argv[i][1]) {
      case 'p':
	ppd=1;
	break;
      case 'd':
	device=1;
	break;
      default:
	printf("Invalid Argument\n");
	usage(argv[0]);
	return -1;
      }
    }
    if (ppd)
      printPpdList();
    else if (device)
      printDevices();
    else
      usage(argv[0]);
  } else {
    char *device_uri = NULL;
    char *ppd_uri = NULL;
    int port = -1;
    char* name = NULL;

    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
	switch (argv[i][1]) {
	case 'D':
	  device_uri = strdup(argv[i+1]);
	  i++;
	  break;
	case 'P':
	  ppd_uri = strdup(argv[i+1]);
	  i++;
	  break;
	case 'n':
	  port = atoi(argv[i+1]);
	  i++;
	  break;
	default:
	  fprintf(stderr,"Unrecognized option\n");
	  usage(argv[0]);
	  exit(-1);
	}
      } else {
	if (name) {
	  printf("NML %s\n", name);
	  usage(argv[0]);
	  exit(-1);
	} else
	  name = strdup(argv[i]);
      }
    }
    printf("Device URI: %s\nPPD/Driver URI: %s\nService Name: %s\n"
	   "IPP Port on localhost: %d\n",
	   device_uri ? device_uri : "Not specified",
	   ppd_uri ? ppd_uri : "Not specified",
	   name ? name : "Not specified",
	   port);
    if (device_uri && ppd_uri && port > 0 && name) {
      printf("Starting IPP printer emulation ...\n");
      attach(device_uri, ppd_uri, name, port);
    } else {
      printf("Unsufficient info to start IPP printer emulation.\n");
      usage(argv[0]);
      return -1;
    }
  }

  return 0;
}
