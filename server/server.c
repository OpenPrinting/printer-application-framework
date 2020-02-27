/*
 *  Printer Application Framework.
 * 
 *  This is the detection and PPD matcher of the Printer Application.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#include "server.h"
#include <sys/socket.h>

static void DEBUG(char* x) {
  static int counter = 0;
  debug_printf("DEBUG[%d]: %s\n", counter++, x);
}

static void escape_string(char* out, char* in, int len) {
  int i;

  for (i = 0; i < (len - 1) && in[i]; i++) {
    if (!isalnum(in[i]))
      out[i] = '-';
    else
      out[i] = in[i];
  }
  out[i] = 0;
}

void cleanup() {
  device_t *dev = cupsArrayFirst(con_devices);

  for (; dev; dev = cupsArrayNext(con_devices)) {
    cupsArrayRemove(con_devices, dev);
    free(dev);
  }
  dev = cupsArrayFirst(temp_devices);
  for (; dev; dev = cupsArrayNext(temp_devices)) {
    cupsArrayRemove(temp_devices, dev);
    free(dev);
  }
  pthread_cancel(hardwareThread);
#ifdef HAVE_AVAHI
  pthread_cancel(avahiThread);
#endif
  pthread_mutex_destroy(&signal_lock);
}

static void kill_main(int sig, siginfo_t *siginfo, void* context) {
  cleanup();
  exit(0);
}

int kill_listeners() {
  struct sigaction kill_act;

  memset(&kill_act, '\0', sizeof(kill_act));
  kill_act.sa_sigaction = &kill_main;
  kill_act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGHUP, &kill_act, NULL) < 0) {
    debug_printf("ERROR: Unable to set cleanup process!\n");
    return 1;
  }
  if(sigaction(SIGINT, &kill_act, NULL) < 0) {
    debug_printf("ERROR: Unable to set cleanup process!\n");
    return 1;
  }
  return 0;
}

void* start_hardware_monitor(void *n) {
  pid_t ppid = getpid();
  monitor_devices(ppid);  /* Listen for usb devices! */
}

#ifdef HAVE_AVAHI
void* start_avahi_monitor(void *n) {
  pid_t ppid = getpid();
  monitor_avahi_devices(ppid);  /* Listen for usb devices! */
}
#endif

device_t* deviceCopy(device_t *in) {
  device_t* out;
  out = calloc(1, sizeof(device_t));
  strcpy(out->device_class, in->device_class);
  strcpy(out->device_info, in->device_info);
  strcpy(out->device_uri, in->device_uri);
  strcpy(out->device_location, in->device_location);
  strcpy(out->device_make_and_model, in->device_make_and_model);
  strcpy(out->device_id, in->device_id);
  strcpy(out->ppd, in->ppd);
  out->eve_pid = in->eve_pid;
  return out;
}

/*
 * get_devices(int, int) - Get list of devices from deviced utility
 */
int get_devices(int insert, int signal) {
  const char  *serverbin; /* ServerBin */
  char        program[2048]; /* Full Path to program */
  char        *argv[7];
  char        *env[7];
  char        name[32], reques_id[16], limit[16],
              timeout[16], user_id[16], options[1024];
  char        serverdir[1024], serverroot[1024], datadir[1024];
  process_t   *process;
  int         process_pid, status;
  char        includes[4096];
  char        isInclude = '+';
  char        tempstr[4095];
  char        arr[NUM_SIGNALS][32] = {"dnssd", "usb", "serial", "parallel"};
  cups_file_t *errlog;
  char *p;

  /*cupsArrayClear(temp_devices);*/
  device_t *temp = cupsArrayFirst(temp_devices);

  for (; temp; temp = cupsArrayNext(temp_devices)) {
    cupsArrayRemove(temp_devices, temp);
    free(temp);
  }

  if ((process = calloc(1, sizeof(process_t))) == NULL) {
    debug_printf("ERROR: Ran Out of Memory!\n");
    return (-1);
  }

  if (!signal) {
    isInclude = '-';
    includes[0] = isInclude;
    char *cj = &includes[1];
    for (int i = 0; i < NUM_SIGNALS; i++) {
      for (int j = 0; j < strlen(arr[i]); j++, cj++)
	*cj=arr[i][j];
      *cj = ',';
      cj++;
    }
    *cj=0;
    cj=0;
  } else {
    includes[0] = isInclude;
    char *cj = &includes[1];
    int index = (signal - 1) / 2;
    for (int j = 0; j < strlen(arr[index]); j++, cj++)
      *cj = arr[index][j];
    if (getenv("SNAP_BACKENDS")) {
      *cj = ',';
      cj++;
      char *bk = strdup(getenv("SNAP_BACKENDS"));
      char *t = bk;
      while (*t) {
	*cj = *t;
	cj++;
	t++;
      }
      free(bk);
    }
    *cj =0;
    cj =0;
  }

  debug_printf("DEBUG: Signal: %s\n", includes);
  strcpy(name, "deviced");
  strcpy(reques_id, DEVICED_REQ);
  strcpy(limit, DEVICED_LIM);
  strcpy(timeout, DEVICED_TIM);
  strcpy(user_id, DEVICED_USE);
  strcpy(options, DEVICED_OPT);

  p = getenv("BINDIR");
  if (p)
    snprintf(program, sizeof(program), "%s/%s", p, name);
  else
    snprintf(program, sizeof(program), "%s%s/%s", snap, BINDIR, name);
  /*snprintf(program, sizeof(program), "deviced");*/
  snprintf(serverdir, sizeof(serverdir),"%s%s", snap, SERVERBIN);
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
    /*if (WIFEXITED(status)) {*/
    while(!parse_line(process));
    /*}*/
    pthread_join(logThread,NULL);
  } else
    fprintf(stdout, "Failed to collect! PID ERROR! %d %s\n",
	    process->pid, strerror(errno));

  if (insert >= 0) {
    if (insert == 1)
      add_devices(con_devices,temp_devices);
    else if (!insert)
      remove_devices(con_devices,temp_devices,includes);
    else {
      add_devices(con_devices,temp_devices);
      remove_devices(con_devices,temp_devices,includes);
    }
  }

  free(process);
  return (0);
}

#if 0
static int parse_line(process_t *backend) {
  char line[2048];
  if (cupsFileGetLine(backend->pipe, line, sizeof(line))) {
    fprintf(stdout, "Line :%s\n\n", line);
    return 0;
  }
  return 1;
}
#endif

int				 /* O - 0 on success, -1 on error */
parse_line(process_t *backend) { /* I - Backend to read from */
  char line[2048],			/* Line from backend */
       temp[2048],			/* Copy of line */
       *ptr,				/* Pointer into line */
       *dclass,				/* Device class */
       *uri,				/* Device URI */
       *make_model,			/* Make and model */
       *info,				/* Device info */
       *device_id,			/* 1284 device ID */
       *location;			/* Physical location */

  if (cupsFileGets(backend->pipe, line, sizeof(line))) {
    /*
     * Each line is of the form:
     *
     *   class URI "make model" "name" ["1284 device ID"] ["location"]
     */

    strlcpy(temp, line, sizeof(temp));
    debug_printf("DEBUG2: %s\n", line);

    /*
     * device-class
     */
    dclass = temp;

    for (ptr = temp; *ptr; ptr ++)
      if (isspace(*ptr & 255))
        break;

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

    /*
     * device-uri
     */

    if (!*ptr)
      goto error;

    for (uri = ptr; *ptr; ptr ++)
      if (isspace(*ptr & 255))
        break;

    while (isspace(*ptr & 255))
      *ptr++ = '\0';

    /*
     * device-make-and-model
     */

    if (*ptr != '\"')
      goto error;

    for (ptr ++, make_model = ptr; *ptr && *ptr != '\"'; ptr ++) {
      if (*ptr == '\\' && ptr[1])
        _cups_strcpy(ptr, ptr + 1);
    }

    if (*ptr != '\"')
      goto error;

    for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

    /*
     * device-info
     */

    if (*ptr != '\"')
      goto error;

    for (ptr ++, info = ptr; *ptr && *ptr != '\"'; ptr ++) {
      if (*ptr == '\\' && ptr[1])
        _cups_strcpy(ptr, ptr + 1);
    }

    if (*ptr != '\"')
      goto error;

    for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

    /*
     * device-id
     */

    if (*ptr == '\"') {
      for (ptr ++, device_id = ptr; *ptr && *ptr != '\"'; ptr ++) {
	if (*ptr == '\\' && ptr[1])
	  _cups_strcpy(ptr, ptr + 1);
      }

      if (*ptr != '\"')
	goto error;

      for (*ptr++ = '\0'; isspace(*ptr & 255); *ptr++ = '\0');

      /*
       * device-location
       */

      if (*ptr == '\"') {
	for (ptr ++, location = ptr; *ptr && *ptr != '\"'; ptr ++) {
	  if (*ptr == '\\' && ptr[1])
	    _cups_strcpy(ptr, ptr + 1);
	}

	if (*ptr != '\"')
	  goto error;

	*ptr = '\0';
      } else
        location = NULL;
    } else {
      device_id = NULL;
      location  = NULL;
    }

    /*
     * Add the device to the array of available devices...
     */
    process_device(dclass, make_model, info, uri, device_id, location);
    /*fprintf(stderr, "DEBUG: Found device \"%s\"...\n", uri);*/

    return (0);
  }

  /*
   * End of file...
   */

  cupsFileClose(backend->pipe);
  backend->pipe = NULL;

  return (-1);

  /*
   * Bad format; strip trailing newline and write an error message.
   */

  error:

  if (line[strlen(line) - 1] == '\n')
    line[strlen(line) - 1] = '\0';

  debug_printf("ERROR: [deviced] Bad line from \"%s\": %s\n",
	       backend->name, line);
  return (0);
}

/*
 *  'compare_devices()' - Compare device uri.
 */
int compare_devices(device_t *d0,
		    device_t *d1) {
  int diff = strcasecmp(d0->device_uri, d1->device_uri);
  /*fprintf(stdout, "%s %s %d\n", d0->device_uri, d1->device_uri, diff);*/
  return diff;
}

static int
process_device(const char *device_class,
	       const char *device_make_and_model,
	       const char *device_info,
	       const char *device_uri,
	       const char *device_id,
	       const char *device_location) {
  device_t *device;

  if ((device = calloc(1, sizeof(device_t))) == NULL) {
    debug_printf("ERROR: Ran out of memory!\n");
    return -1;
  }

  if (device_make_and_model) {
    if (!strncasecmp(device_make_and_model, "Unknown", 7)) {
      free(device);
      return -2;
    }
  }

  if (device_uri)
    strlcpy(device->device_uri, device_uri, sizeof(device->device_uri));
  if (device_class)
    strlcpy(device->device_class, device_class, sizeof(device->device_class));
  if (device_make_and_model)
    strlcpy(device->device_make_and_model, device_make_and_model,
	    sizeof(device->device_make_and_model));
  if (device_info)
    strlcpy(device->device_info, device_info, sizeof(device->device_info));
  if (device_id)
    strlcpy(device->device_id, device_id, sizeof(device->device_id));
  if (device_location)
    strlcpy(device->device_location, device_location,
	    sizeof(device->device_location));

  if (cupsArrayFind(temp_devices, device))
    free(device);
  else {
    int res = cupsArrayAdd(temp_devices, device); /* Do we need device limit? */
  }

  return 0;
}

void add_devices(cups_array_t *con, cups_array_t *temp) {
  device_t *dev = cupsArrayFirst(temp);
  char ppd[1024];

  for(;dev;dev=cupsArrayNext(temp)) {
    if (dev == NULL)
      break;
    if (!cupsArrayFind(con, dev)) {
      debug_printf("DEBUG: Getting PPD! |%s|%s|%s|\n",
		   dev->device_make_and_model, dev->device_uri, dev->device_id);
      int ret = get_ppd(ppd, sizeof(ppd), dev->device_make_and_model,
			sizeof(dev->device_make_and_model),
			dev->device_id, sizeof(dev->device_id),
			dev->device_uri);
      if (ret >= 0) {
        strlcpy(dev->ppd, ppd, sizeof(dev->ppd));
        debug_printf("DEBUG: PPD LOC: %s\n", dev->ppd);
        device_t* newDev = deviceCopy(dev);
        cupsArrayAdd(con, newDev);
        debug_printf("DEBUG: Adding Printer: %s\n", newDev->device_uri);
        start_ippeveprinter(newDev);
      } else {
	dev->ppd[0] = '\0';
        debug_printf("DEBUG: PPD not found, not adding this printer!\n");
	device_t* newDev = deviceCopy(dev);
	cupsArrayAdd(con, newDev);
      }
    }
  }
}

int getBackend(char *uri, char *backend, int bklen) {
  char userpass[256],		/* username:password (unused) */
       host[256],		/* Hostname or IP address */
       resource[256];		/* Resource path */
  int  port;			/* Port number */
    
  if (uri == NULL) {
    backend = NULL;
    return -1;
  }
  if (uri[0] == '\"') {
    int len = strlen(uri);
    for(int i = 0; i < len - 2; i++)
      uri[i] = uri[i+1];
    uri[len - 2] = 0;
  }
  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, backend, bklen,
		      userpass, sizeof(userpass), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_STATUS_OK) {
    debug_printf("ERROR: %s %s\n", uri, backend); 
    return -1;
  }

  return 0;
}

void remove_devices(cups_array_t *con, cups_array_t *temp, char *includes) {
  device_t *dev = cupsArrayFirst(con);
  char ppd[128];
  int inc = 1;
  if (includes[0] == '-') inc = 0;
  for (; dev; dev = cupsArrayNext(con)) {
    char backend[32];
    if (getBackend(dev->device_uri, backend, sizeof(backend)))
      continue;
    if (inc) {
      if (!strstr(includes, backend))
        continue;
    } else {
      if (strstr(includes, backend))
        continue;
    }
    if (cupsArrayFind(temp, dev) == NULL) {
      if (dev->ppd[0] != '\0') {
	remove_ppd(dev->ppd);
	debug_printf("DEBUG: Removing Printer: %s\n", dev->device_id);
	kill_ippeveprinter(dev->eve_pid);
	pthread_join(dev->errlog, NULL);
      } else
	debug_printf("DEBUG: Unsupported printer disappeared: %s\n",
		     dev->device_id);
      cupsArrayRemove(con, dev);
      device_t *tt = dev;   // Do we need this?
      free(tt);
    }
  }
}

/*
 * Use language to filter???
 */

static int
get_ppd(char* ppd, int ppd_len,            /* O- */ 
        char *make_and_model, int make_len,
        char *device_id, int dev_len, char *device_uri) /* I- */
{
  const char *serverbin;
  char program[2048];
  char *argv[6];
  char name[16], operation[8], request_id[4], limit[5], options[1024];
  char ppd_uri[128];
  char ppd_name[1024];  /* full ppd path */
  char escp_model[256];
  char *envp[6];
  char datadir[1024], serverdir[1024], cachedir[1024];
  cups_file_t *errlog;
  process_t *process;
  int        process_pid, status;

  if ((process = calloc(1, sizeof(process_t))) == NULL) {
    debug_printf("ERROR: Ran Out of Memory!\n");
    return (-1);
  }
  strcpy(name, "cups-driverd");
  strcpy(operation, "list");
  strcpy(request_id, "0");
  strcpy(limit, "1");
  snprintf(options, sizeof(options),
	   "ppd-make-and-model=\'%s\' ppd-device-id=\'%s\'",
	   make_and_model, device_id);
  /*snprintf(options, sizeof(options), "ppd-make-and-model=\'HP\'");*/

  snprintf(datadir, sizeof(datadir), "%s%s", snap, DATADIR);
  snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
  snprintf(cachedir, sizeof(cachedir), "%s", tmpdir);

  setenv("CUPS_DATADIR", datadir, 1);
  setenv("CUPS_SERVERBIN", serverdir, 1);
  setenv("CUPS_CACHEDIR", cachedir, 1);
  /*if((serverbin = getenv("SERVERBIN")) == NULL)
    serverbin = CUPS_SERVERBIN;*/
  snprintf(program, sizeof(program), "%s/daemon/%s", serverdir, name);

  argv[0] = (char*) name;
  argv[1] = (char*) operation;
  argv[2] = (char*) request_id;
  argv[3] = (char*) limit;
  argv[4] = (char*) options;
  argv[5] = NULL;

#if 0
  envp[0] = (char*) datadir;
  envp[1] = (char*) serverdir;
  envp[2] = (char*) cachedir;
  envp[3] = NULL;
#endif

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
  if ((process_pid = waitpid(process->pid, &status, 0)) > 0) {
    if(WIFEXITED(status)) {
      /*do {*/
      if (get_ppd_uri(ppd_uri, process)) { /* All we need is a single line! */
	free(process);
	pthread_join(logThread, NULL);
	return (-1);
      }
      /*fprintf(stdout,"PPD-URI: %s\n",ppd_uri);*/
      /*} while(_cupsFilePeekAhead(process->pipe, '\n'));*/
    }
    pthread_join(logThread,NULL);
  }

  strcpy(operation, "cat");
  argv[2] = (char*) ppd_uri;
  argv[3] = NULL;
  argv[4] = NULL;

  if ((process->pipe = cupsdPipeCommand2(&(process->pid), program,
					 argv, &errlog, 0)) == NULL) {
    debug_printf("ERROR: Unable to execute cups-driverd!\n");
    free(process);
    cupsFileClose(errlog);
    return (-1);
  }
  logFromFile2(&logThread, errlog);

  char ppdn[1024];
  snprintf(ppdn, sizeof(ppdn), "%s-%s", make_and_model, device_uri);
  escape_string(escp_model, ppdn, sizeof(ppdn));
  char ppd_folder[2048];
  snprintf(ppd_folder, sizeof(ppd_folder), "%s/ppd", tmpdir);
  if (mkdir(ppd_folder, 0777) == -1 && errno != EEXIST)
    debug_printf("ERROR: Cannot create directory %s: %s\n",
		 ppd_folder, strerror(errno));
  snprintf(ppd_name, sizeof(ppd_name), "%s/ppd/%s.ppd", tmpdir, escp_model);
  cups_file_t* tempPPD;
  if ((tempPPD = cupsFileOpen(ppd_name, "w")) == NULL) {
    debug_printf("ERROR: Cannot create temporary PPD!\n");
    free(process);
    return (-1);
  }
  int counter = print_ppd(process,tempPPD);

  if ((process_pid = waitpid(process->pid, &status, 0)) > 0) {
#if 0
    if (WIFEXITED(status)) {
      int st =0,counter=0;
      counter = print_ppd(process,tempPPD);
      /*while((st = print_ppd(process, tempPPD)) > 0) counter++;*/
    }
#endif
    pthread_join(logThread, NULL);
  }

  cupsFileClose(tempPPD);
  free(process);
  strncpy(ppd, ppd_name, ppd_len);
  return 0;
}

int get_ppd_uri(char* ppd_uri, process_t* backend) {
  char line[2048];

  if (cupsFileGets(backend->pipe, line, sizeof(line))) {
    sscanf(line, "%s", ppd_uri);
    cupsFileClose(backend->pipe);
    return 0;
  }
  cupsFileClose(backend->pipe);
  return 1;
}

int print_ppd(process_t* backend, cups_file_t *temp) {
  int counter = 0;
  char line[2048];

  while (cupsFileGets(backend->pipe, line, sizeof(line))) {
    cupsFilePrintf(temp, "%s\n", line);
    counter++;
  }
  cupsFileClose(backend->pipe);
  return counter;
}

int remove_ppd(char* ppd) {
  return unlink(ppd);
}

int start_ippeveprinter(device_t *dev) {
  pid_t pid = 0, ppid = 0;
  ppid = getpid();
  int pfd[2];

  if (pipe(pfd))
    return -1;

  if (fcntl(pfd[0], F_SETFD, fcntl(pfd[0], F_GETFD) | FD_CLOEXEC)) {
    close(pfd[0]);
    close(pfd[1]);
    return -1;
  }

  if (fcntl(pfd[1], F_SETFD, fcntl(pfd[1], F_GETFD) | FD_CLOEXEC)) {
    close(pfd[0]);
    close(pfd[1]);
    return -1;
  }

  if ((pid = fork()) < 0) {
    close(pfd[0]);
    close(pfd[1]);
    return -1;
  } else if (pid == 0) {
    int status = 0;
    if ((status = prctl(PR_SET_PDEATHSIG, SIGTERM)) < 0) {
      perror(0);    /* Unable to set prctl */
      exit(1);
    }
    if(getppid() != ppid)
      exit(0);      /* Parent as exited already! */

    char *argv[17];
    char name[2048], device_uri[2048], ppd[1024], make_and_model[512],
         command[1024], pport[8], location[3096], service_name[1024],
      package[64], identifier[256], backend[32], serial[64], pdls[2048];
    char datadir[1024], serverdir[1024], cachedir[1024], cmdline[2048];
    char *envp[2];
    char LD_PATH[512];
    char *p, *q, *r, *s;
    char *field[] = {"SN", "SERN", "serial", "hostname", "ip", NULL};
    int i, len;

    snprintf(datadir, sizeof(datadir), "%s%s", snap, DATADIR);
    snprintf(serverdir, sizeof(serverdir), "%s%s", snap, SERVERBIN);
    snprintf(cachedir, sizeof(cachedir), "%s", tmpdir);

    setenv("CUPS_DATADIR", datadir, 1);
    setenv("CUPS_SERVERBIN", serverdir, 1);
    setenv("CUPS_CACHEDIR", cachedir, 1);
    setenv("DEVICE_URI", device_uri, 1);
    setenv("PRINTER", dev->device_make_and_model, 1);

    snprintf(name, sizeof(name), "%s%s/ippeveprinter", snap, SBINDIR);
    if(dev == NULL)
      exit(1);
    if(dev->device_uri)
      snprintf(device_uri, sizeof(device_uri), "\"%s\"", dev->device_uri);
    if(dev->ppd)
      snprintf(ppd, sizeof(ppd), "%s", dev->ppd);
    snprintf(pport, sizeof(pport), "%d", getport());

    p = getenv("BINDIR");
    if (p)
      snprintf(command, sizeof(command), "%s/ippprint", p);
    else
      snprintf(command, sizeof(command), "%s%s/ippprint", snap, BINDIR);

    p = getenv("PDLS");
    if (p)
      strlcpy(pdls, p, sizeof(pdls));
    else
      pdls[0] = '\0';

    /* Make the service name of our IPP printer emulation unique for the case
       that we have multiple printers of the same model */
    p = getenv("PACKAGENAME");
    if (p)
      snprintf(package, sizeof(package), "%s, ", p);
    else
      package[0] = '\0';

    service_name[0] = '\0';
    s = strchr(dev->device_uri, ':');
    strlcpy(backend, dev->device_uri, s - dev->device_uri + 1);
    for (i = 0; i < strlen(backend); i ++)
      backend[i] = toupper(backend[i]);
    serial[0] = '\0';
    if ((q = strchr(dev->device_info, '[')) && (r = strchr(q, ']'))
	&& (len = r - q - 1) > 0)
      strlcpy(serial, q + 1, len + 1);
    if (serial[0] == '\0')
      for (i = 0; ; i ++) {
	p = field[i];
	if (p == NULL) break;
	if ((q = strcasestr(dev->device_uri, p)) != NULL &&
	    (q == dev->device_uri || *(q - 1) == '?' ||  *(q - 1) == '&') &&
	    (*(q + strlen(p)) == '=')) {
	  q += strlen(p) + 1;
	  len = 0;
	  if ((r = strchr(q, '&')) || (len = strlen(q))) {
	    if (r) len = r - q;
	    if (len > sizeof(serial) - 1)
	      strlcpy(serial, q, sizeof(serial));
	    else
	      strlcpy(serial, q, len + 1);
	    break;
	  }
	}
      }
    if (serial[0] == '\0')
      for (i = 0; ; i ++) {
	p = field[i];
	if (p == NULL) break;
	if ((q = strcasestr(dev->device_id, p)) != NULL &&
	    (q == dev->device_id || *(q - 1) == ';') &&
	    (*(q + strlen(p)) == ':')) {
	  q += strlen(p) + 1;
	  len = 0;
	  if ((r = strchr(q, ';')) || (len = strlen(q))) {
	    if (r) len = r - q;
	    if (len > sizeof(serial) - 1)
	      strlcpy(serial, q, sizeof(serial));
	    else
	      strlcpy(serial, q, len + 1);
	    break;
	  }
	}
      }
    snprintf(identifier, sizeof(identifier), " (%s%s%s%s)", package, backend,
	     serial[0] ? ", " : "", serial);
    strlcpy(service_name, dev->device_make_and_model, sizeof(service_name));
    if (63 - strlen(service_name) < strlen(identifier))
      r = service_name + 63 - strlen(identifier);
    else
      r = service_name + strlen(service_name);
    strlcpy(r, identifier, strlen(identifier) + 1);
    service_name[63] = '\0';

    snprintf(location, sizeof(location),
	     "Printer Application, Original Device Info: %s",
	     dev->device_info);

    char printer_name[512];
    char scheme[10];
    getBackend(dev->device_uri, scheme, sizeof(scheme));
    snprintf(printer_name, sizeof(printer_name), "%s",
	     dev->device_make_and_model);
    printer_name[sizeof(printer_name) - 1] = 0;
    escape_string(make_and_model, printer_name, sizeof(printer_name));
    i = 0;
    argv[i++] = (char*)name;
    argv[i++] = "-P";
    argv[i++] = (char*)ppd;
    argv[i++] = "-c";
    argv[i++] = (char*)command;
    if (pdls[0]) {
      argv[i++] = "-f";
      argv[i++] = (char*)pdls;
    }
    argv[i++] = "-p";
    argv[i++] = (char*)pport;
    argv[i++] = "-l";
    argv[i++] = (char*)location;
#if 0
    argv[i++] = "-K";
    argv[i++] = (char*)tmpdir;
    argv[i++] = "-n";
    argv[i++] = strdup("localhost");
#endif
    argv[i++] = (char*)service_name;
    argv[i++] = NULL;

    char *logdir = logdirname();
    /*dup2(1, 2);*/
    char printerlogs[1024];
    snprintf(printerlogs, sizeof(printerlogs), "%s/printer.logs", logdir);
    /*int logfd = open(printerlogs, O_CREAT,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    close(logfd);
    logfd = open(printerlogs, O_WRONLY | O_APPEND);*/
    strlcpy(cmdline, "DEBUG: Launching IPP printer emulator: ", sizeof(cmdline));
    p = cmdline + strlen(cmdline);
    q = cmdline + sizeof(cmdline) - 1;
    for (i = 0; ; i++) {
      if (argv[i] == NULL || p >= q) break;
      if (strchr(argv[i], ' ') || strchr(argv[i], '\t')) {
	*p = '\'';
	p++;
      }
      if (strlen(argv[i]) > q - p) {
	strlcpy(p, argv[i], q-p);
	break;
      } else {
	strlcpy(p, argv[i], strlen(argv[i]) + 1);
	p += strlen(argv[i]);
      }
      if (p >= q) break;
      if (strchr(argv[i], ' ') || strchr(argv[i], '\t')) {
	*p = '\'';
	p++;
      }
      if (p >= q) break;
      *p = ' ';
      p++;
    }
    *p = '\0';
    debug_printf("%s\n", cmdline);
    /*if (logfd > 0) {
      dup2(logfd, 2);
      dup2(logfd, 1);
      close(logfd);
      }*/
    dup2(pfd[1], 2);
    dup2(pfd[1], 1);
    close(pfd[1]);

    execvp(argv[0], argv);
    exit(0);
  }

  close(pfd[1]);
  logFromFd(&(dev->errlog), pfd[0]);

  if (dev)
    dev->eve_pid = pid;

  return pid;
}

int getport() {
  int port = 8000;

  for (; port < 9000; port++) {
    int sd = 0;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
      continue;
    int true = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    int t;
    if ((t = bind(sd, (struct sockaddr*)&server,
		  sizeof(struct sockaddr))) >= 0) {
      /*fprintf(stderr, "IN: %d %d\n", sd, t);*/
      close(sd);
      break;
    }
    close(sd);
    /*fprintf(stderr, "%d %d\n", sd, t);*/
  }
  return port;
}

static int kill_ippeveprinter(pid_t pid) {
  int status;

  if (pid > 0) {
    debug_printf("DEBUG: Killing ippeveprinter: %d\n", pid);
    kill(pid, SIGINT);
    if (waitpid(pid, &status, 0) < 0) {
      debug_printf("ERROR: WAITPID Error!\n");
      return -1;
    }
  }
  return 0;
}
