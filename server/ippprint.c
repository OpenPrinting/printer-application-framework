/*
 *  Printer Application Framework.
 *
 *  ippprint program is executed by the ippeveprinter. This program is
 *  responsible for applying the filter chain and sending the job to
 *  the backend.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#include "ippprint.h"

extern char **environ;

char *tmpdir; //SNAP_COMMON

/*
 * getUserId() - Get uid for a given username.
 */
static int getUserId(const char *username) {
  struct passwd *p;

  if ((p = getpwnam(username)) == NULL)
    return -1;
  return (int)p->pw_uid;
}

void ini() {
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
}

/*
 * getFilterPath() - Get path to required filter.
 * 
 * It checks for filters in SERVERBIN/filter folder and its sub directories
 * up to a depth 1. This allows us to create a symbolic link in
 * SERVERBIN/filter to CUPS filter directories.
 * 
 * in   - Filter name
 * *out - Full path of the Filter
 * 
 * Return:
 *  0    - Success
 *  != 0 - Error
 */
static int getFilterPath(char *in, char **out) {
  *out = NULL;
  char path[2048];
  cups_dir_t* dir;
  cups_dentry_t* dent;
  char *serverbin, *snap;

  serverbin = getenv("CUPS_SERVERBIN");
  if (serverbin == NULL) {
    if ((snap = getenv("SNAP")) == NULL)
      snap = "";
    serverbin = "/usr/lib/cups";
  } else
    snap = "";
  snprintf(path, sizeof(path), "%s%s/filter/%s", snap, serverbin, in);
      /* Check if we can directly access filter */

  if ((access(path, F_OK | X_OK) != -1) && fileCheck(path)) {
    *out = strdup(path);
    return 0;
  }

  snprintf(path, sizeof(path), "%s%s/filter", snap, serverbin);
  dir = cupsDirOpen(path);

  while ((dent = cupsDirRead(dir))) { /*Check only upto one level*/
    char *filename = dent->filename;
    if (S_ISDIR(dent->fileinfo.st_mode)) {
      snprintf(path, sizeof(path), "%s%s/filter/%s/%s", snap, serverbin,
	       filename, in);
      if ((access(path, F_OK | X_OK) != -1) && fileCheck(path)) {
        *out = strdup(path);
        break;
      }
    }
  }
  cupsDirClose(dir);

  if(*out)
    return 0;

  return -1;
}

/*
 * getFilterPaths() - For a given list of filters, find corresponding full
 * paths of those filters.
 * 
 * Returns:
 * 0  - Success
 * !0 - Error 
 */
static int getFilterPaths(cups_array_t *filter_chain,
			  cups_array_t **filter_fullname) {
  *filter_fullname = cupsArrayNew(NULL, NULL);

  char *in, *out;
  filter_t *currFilter;

  for (currFilter = cupsArrayFirst(filter_chain); currFilter;
       currFilter = cupsArrayNext(filter_chain)) {
    in = currFilter->filter;
    if (strcmp(in, "-") == 0)    /* Empty filter */
      continue;
    if (getFilterPath(in, &out) == -1)
      return -1;
    filter_t* temp = filterCopy(currFilter);
    free(temp->filter);
    temp->filter = out;
    cupsArrayAdd(*filter_fullname, temp);
  }
  return 0;
}

/*
 * createOptionsArray() - Create Options array from env variables.
 * 
 * Returns:
 * 0  - Success
 * !0 - Error
 */
static int createOptionsArray(char *op) { /*O-*/
  sprintf(op, "h");
  return 0;
}

/*
 * executeCommand() - Execute a filter
 * 
 * It takes input from inPipe fd and writes to outPipe fd.
 * 
 * Return - 
 * -1 - Error
 * pid of the filter - Success
 */
static pid_t executeCommand(int inPipe, int outPipe, filter_t *filter, int i) {
  pid_t pid;
  char *filename = filter->filter;

  debug_printf("DEBUG: Executing Command: %s\n", filename);
  if ((pid = fork()) < 0)
    return -1;
  else if (pid == 0) {
    dup2(inPipe, 0);
    dup2(outPipe, 1);
    close(inPipe);
    close(outPipe);

    char *argv[10];
    int uid = getUserId(getenv("IPP_JOB_ORIGINATING_USER_NAME"));
    char userid[64];

    snprintf(userid, sizeof(userid), "%d", (uid < 0 ? 1000 : uid));

    argv[0] = strdup(filename);
    argv[1] = strdup(getenv("IPP_JOB_ID"));         /* Job ID */
    argv[2] = strdup(userid);                       /* User ID */
    argv[3] = strdup(getenv("IPP_JOB_NAME"));       /* Title */
    argv[4] = strdup(getenv("IPP_COPIES_DEFAULT")); /* Copies */
    argv[5] = strdup("\"\"");                       /* Options */
    argv[6] = NULL;
    char newpath[1024];
    setenv("OUTFORMAT", filter->dest->typename, 1);
    execvp(*argv, argv);
    exit(0);
  } else {
    close(inPipe);
    close(outPipe);
    return pid;
  }
  exit(0);
}

/*
 * applyFilterChain() - Apply a series of filters given by *filters.
 * 
 * The input file name is given in inputFile.
 * Output File will be written to finalFile.
 * namelen - length of finalFile container.
 * 
 * Returns:
 * 0 - Success
 * !0 - Error 
 */
static int applyFilterChain(cups_array_t* filters, char *inputFile,
			    char *finalFile, int namelen) {
  int numPipes = cupsArrayCount(filters) + 1;
  char outName[1024];
  int pipes[2 * MAX_PIPES];
  int killall = 0;
  int res = 0;
  pid_t pd;
  int status;

  cups_array_t* children=cupsArrayNew(NULL, NULL);

  if (children == NULL) {
    debug_printf("ERROR: Ran out of memory!\n");
    return -1;
  }

  if (numPipes > MAX_PIPES) {
    debug_printf("ERROR: Too many Filters!\n");
    return -1;
  }

  snprintf(outName, sizeof(outName), "%s/printjob.XXXXXX", tmpdir);
  debug_printf(outName);
  for (int i = 0; i < numPipes; i++) {
    res = pipe(pipes + 2 * i); /* Try Opening Pipes */
    if(res)                    /* Unable to open Pipes! */
      return -1;
  }

  int inputFd = open(inputFile, O_RDONLY); /* Open Input file */
  if (inputFd < 0) {
    if (errno == EACCES)
      debug_printf("ERROR: Permission Denied! Unable to open file: %s\n",
		   inputFile);
    else
      debug_printf("ERROR: ERRNO: %d Unable to open file: %s\n",
		   errno, inputFile);
    return -1;
  }

  int outputFd = mkstemp(outName);
  if(outputFd<0)
  {
    debug_printf("ERROR: ");
    if(errno==EACCES)
      debug_printf("Permission Denied!");
    if(errno==EEXIST)
      debug_printf("Directory is full! Used all temporary file names! ");
    debug_printf("Unable to open temporary file!\n");
    return -1;
  }

  strncpy(finalFile, outName, namelen); /* Write temp filename to finalFile */

  dup2(inputFd, pipes[0]);     /* Input file */
  close(inputFd);
  
  dup2(outputFd, pipes[2 * numPipes - 1]);  /* Output(temp file) */
  close(outputFd);
  
  for(int i = 0; i < numPipes - 1; i++) {
    filter_t *tempFilter = ((filter_t*)cupsArrayIndex(filters, i));
    pid_t *pd = calloc(1, sizeof(pid_t));
    *pd = executeCommand(pipes[2 * i], pipes[2 * i + 3],
			 tempFilter, i);  /* Execute the filter */
    if (pd < 0) {
      debug_printf("ERROR: Unable to execute filter %s!\n",
		   tempFilter->filter);
      killall = 1;  /* Chain failed kill all filters */
      goto error;
    }
    cupsArrayAdd(children, pd);
  }
  for (int i = 0; i < numPipes; i++) {    /* Close all pipes! */
    close(pipes[2 * i]);
    close(pipes[2 * i + 1]);
  }

  while ((pd = waitpid(-1, &status, 0)) > 0) {
    /* Wait for all child processes to exit */
    if (WIFEXITED(status)) {
      int es = WEXITSTATUS(status);
      debug_printf("%s: Filter Process %d exited with status %d\n",
		   (es ? "ERROR" : "DEBUG"), pd, es); 
      if (es) {
        killall = 1;    /* (Atleast) One filter failed. kill entire chain. */
        goto error;
      }
    }
  }

  debug_printf("DEBUG: Applied Filter Chain!\n");

error:
  if (killall) {
    pid_t *temPid;
    for(temPid = cupsArrayFirst(children); temPid;
	temPid = cupsArrayNext(children)) {
      kill(*temPid, SIGTERM);
    }
    return -1;
  }
  return 0;
}

/*
 * getDeviceScheme() - Get scheme(backend) from device_uri env variable.
 * 
 * It writes device_uri to device_uri_out.
 * It writes scheme to scheme.
 * 
 * Returns:
 * 
 * 0 - Success
 * !0 - Error
 */
static int getDeviceScheme(char **device_uri_out, char *scheme, int schemelen) {
  char *device_uri = NULL;
  int i;
  char userpass[256],		/* username:password (unused) */
       host[256],		/* Hostname or IP address */
       resource[256];		/* Resource path */
  int	port;			/* Port number */

  char *p = getenv("DEVICE_URI");
  if (p)
    device_uri = strdup(p);
  /*device_uri =
    strdup("\"hp:/usb/OfficeJet_Pro_6960?serial=TH6CL621KN\"");*/
  debug_printf("DEBUG: DEVICE_URI env variable: %s\n", device_uri);

  if (device_uri == NULL) {
    *device_uri_out = NULL;
    scheme = NULL;
    return -1;
  }

  if (device_uri[strlen(device_uri) - 1] == '\"')
    device_uri[strlen(device_uri) - 1] = '\0';     /* Remove last \" */

  if (device_uri[0] == '\"')
    for (i = 0; i < strlen(device_uri); i++)       /* Remove first \" */
      device_uri[i]=device_uri[i+1];

  *device_uri_out = device_uri;
  debug_printf("DEBUG: Device URI to be used: %s\n", *device_uri_out);

  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, schemelen, 
		      userpass, sizeof(userpass), host, sizeof(host), &port,
		      resource, sizeof(resource)) < HTTP_URI_STATUS_OK) {
    debug_printf("ERROR: [Job %d] Bad device URI \"%s\".\n", 0, device_uri);
    *device_uri_out = NULL;
    return -1;
  }

  return 0;
}

static int print_document(char *scheme, char *uri, char *filename) {
  char backend[2048];
  pid_t pid;
  int status;
  char *serverbin, *snap;

  serverbin = getenv("CUPS_SERVERBIN");
  if (serverbin == NULL) {
    if ((snap = getenv("SNAP")) == NULL)
      snap = "";
    serverbin = "/usr/lib/cups";
  } else
    snap = "";

  snprintf(backend, sizeof(backend), "%s%s/backend/%s",
	   snap, serverbin, scheme);
  debug_printf("DEBUG: Backend: %s %s\n", backend, uri);

  /*
   * Check file permissions and do fileCheck().
   */
  if ((access(backend, F_OK | X_OK) == -1) && fileCheck(backend)) {
    debug_printf("ERROR: Unable to execute backend %s\n", scheme);
    return -1;
  }
  
  /*dup2(fileno(sout), 2);*/ /* sout -> File logs */
  
  if ((pid = fork()) < 0) {
    debug_printf("ERROR: Unable to fork!\n");
    return -1;
  } else if (pid == 0) {
    char userid[64];
    int uid;
    char *job_user, *job_id, *job_name, *job_copies;
    job_user = getenv("IPP_JOB_ORIGINATING_USER_NAME");
    job_id = getenv("IPP_JOB_ID");
    job_name = getenv("IPP_JOB_NAME");
    job_copies = getenv("IPP_COPIES_DEFAULT");
    if (job_user)
      uid = getUserId(job_user);
    else {
      debug_printf("DEBUG: IPP_JOB_ORIGINATING_USER_NAME not supplied, using 1000\n");
      uid = 1000;
    }
    snprintf(userid, sizeof(userid), "%d", (uid < 0 ? 1000 : uid));
    if (job_id == NULL) {
      debug_printf("DEBUG: IPP_JOB_ID not supplied, using 1\n");
      job_id = strdup("1");
    }
    if (job_name == NULL) {
      debug_printf("DEBUG: IPP_JOB_NAME not supplied, using \"Untitled\"\n");
      job_name = strdup("Untitled");
    }
    if (job_copies == NULL) {
      debug_printf("DEBUG: IPP_COPIES_DEFAULT not supplied, using 1\n");  
      job_copies = strdup("1");
    }
    debug_printf("DEBUG: Executing backend: %s %s %s %s %s %s\n",
		 uri, job_id, userid, job_name, job_copies,
		 filename);
    char *argv[10];
    argv[0] = strdup(uri);
    argv[1] = strdup(job_id);                       /* Job ID */
    argv[2] = strdup(userid);                       /* User ID */
    argv[3] = strdup(job_name);                     /* Title */
    argv[4] = strdup(job_copies);                   /* Copies */
    argv[5] = strdup("\"\"");                       /* Options */
    argv[6] = strdup(filename);
    argv[7] = NULL;

    execvp(backend, argv);
  }

  while ((pid = waitpid(-1, &status, 0)) > 0) {
    if (WIFEXITED(status)) {
      int er = WEXITSTATUS(status);
      debug_printf("%s: Process %d exited with status %d\n",
		   (er ? "ERROR" : "DEBUG"), pid, er);
      return er;
    }
  }
  return 0;
}

static int delete_temp_file(char *filename) {
  /*return unlink(filename);*/
  return 0;
}

void testApplyFilterChain() {
  cups_array_t* t = cupsArrayNew(NULL,NULL);
  cupsArrayAdd(t, "1");
  cupsArrayAdd(t, "1");
  cupsArrayAdd(t, "1");
  cupsArrayAdd(t, "1");
  char inputFile[1024];
  snprintf(inputFile, sizeof(inputFile), "%s/logs.txt", tmpdir);
  /*char *outFile;*/
  applyFilterChain(t, inputFile, NULL, 0);
}

int main(int argc, char *argv[]) {
  ini();
  setenv("LOG_NAME", "ippprint.txt", 1);
  char **s = environ;
  for (; *s; ) {
    debug_printf("DEBUG: %s\n", *s);
    s = (s + 1);
  }
  char device_scheme[32], *device_uri;
  char *ppdname = NULL;
  char *output_type = NULL;
  char *content_type = NULL;
  char finalFile[1024];
  /*fprintf(stderr, "WTF???\n");*/
  if (getDeviceScheme(&device_uri, device_scheme, sizeof(device_scheme)) != 0) {
    debug_printf("ERROR: No device URI supplied via DEVICE_URI environment variable\n");
    return -1;
  }
  setenv("DEVICE_URI", device_uri, 1);
  debug_printf("DEBUG: Device_scheme: %s %s\n", device_scheme, device_uri);
  
  int isPPD = 1, isOut = 1;

  if (argc != 2) {
    debug_printf("ERROR: No input file name supplied! Usage: ippprint FILE\n");
    return -1;
  }

  /*exit(0);*/
  char *inputFile = strdup(argv[1]); /* Input File */

  if (getenv("CONTENT_TYPE") == NULL) {
    debug_printf("ERROR: Environment variable CONTENT_TYPE not set!\n");
    return 0;
  }
  if (getenv("PPD") == NULL)
    isPPD = 0;
  if (getenv("OUTPUT_TYPE") == NULL)
    isOut = 0;
  if (isPPD)
    ppdname = strdup(getenv("PPD"));

  content_type = strdup(getenv("CONTENT_TYPE"));

  if (isOut)
    output_type = strdup(getenv("OUTPUT_TYPE"));

#if 0
  ppdname = strdup("/home/dj/Desktop/HP-OfficeJet-Pro-6960.ppd");
  setenv("PPD", ppdname, 1);
  content_type = strdup("application/pdf");

  setenv("IPP_JOB_ORIGINATING_USER_NAME", "dj", 1);
  setenv("IPP_JOB_ID", "1", 1);
  setenv("IPP_JOB_NAME", "test", 1);
  setenv("IPP_COPIES_DEFAULT", "1", 1);
  setenv("PRINTER", "HP Officejet 6960", 1);
#endif

  cups_array_t *filter_chain, *filterfullname;
  filter_t *paths;
  filter_t *tempFilter;

  int res = get_ppd_filter_chain(content_type, output_type, ppdname,
				 &filter_chain);

  if (res < 0) {
    debug_printf("ERROR: Unable to create filter chain!\n");
    exit(-1);
  }
  debug_printf("DEBUG: Filter Chain for the job:\n");
  for (tempFilter = cupsArrayFirst(filter_chain); tempFilter;
       tempFilter = cupsArrayNext(filter_chain))
    debug_printf("DEBUG: Filter: %s\n", tempFilter->filter);
  res = getFilterPaths(filter_chain, &filterfullname);
  if (res < 0) {
    debug_printf("ERROR: Unable to find required filters!\n");
    exit(-1);
  }
  for (paths = cupsArrayFirst(filterfullname); paths;
       paths = cupsArrayNext(filterfullname))
    debug_printf("DEBUG: Filter full path: %s\n", paths->filter);
  res = applyFilterChain(filterfullname, inputFile, finalFile,
			 sizeof(finalFile));
  /*debug_printf("Final File Name: %s\n", finalFile);*/

  if (res < 0) {
    debug_printf("ERROR: Filter Chain Error!\n");
    exit(-1);
  }

  if (device_uri) {
    res = print_document(device_scheme, device_uri, finalFile);
    if (res == 0)
      debug_printf("DEBUG: Successfully printed file!\n");
  }

  delete_temp_file(finalFile);

  debug_printf("*****************************************************\n");
  
  if(res)
    return -1;
  return res;
}
