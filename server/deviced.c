/*
 *  Printer Application Framework.
 * 
 *  deviced utility invokes all the available backends and get device list.
 *  It is largely based on the cups-deviced utility of cups package.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */
#include "deviced.h"

#define MAX_BACKENDS 200

typedef struct
{
  char *name;
  int pid,status;
  cups_file_t *pipe;
  int count;
} backend_t;

int active_backends,num_backends;
int normal_user = 1;
int dead_children = 1;

static backend_t backends[MAX_BACKENDS];
static struct pollfd backend_fds[MAX_BACKENDS];
static void sigchld_handler(int sig,siginfo_t *siginfo, void*context);
static double get_current_time(void);
static int get_device(backend_t *backend);
static int start_backend(const char* filename, int root);

static int signal_listeners()
{
  struct sigaction act;
  memset(&act,'\0',sizeof(act));
  act.sa_sigaction = &sigchld_handler;
  act.sa_flags = SA_SIGINFO;
  if(sigaction(SIGCHLD,&act,NULL)<0){
    fprintf(stderr,"ERROR: Unable to start SIGCHLD Handler!\n");
    exit(1);
  }
}
/*
 * main()
 * Do we need exclude-include???
 */

int main(int argc,
        char *argv[] )
{
  int i,
    device_limit,
    timeout;
  char *server_bin,
    dirname[1024]; 
  char includes[4096];    //Include-Exclude String
  int inc_length=0,
      exc_length=0;
  int isInclude = 1;
  char inc_backends[MAX_BACKENDS][1024];  //Include Backends
  char exc_backends[MAX_BACKENDS][1024];  //Exclude Backends
  cups_dir_t *dir;    // FD
  cups_dentry_t *dent;
  double end_time,current_time;
  if(argc<4||argc>4){
    fprintf(stderr,"Usage: %s limit timeout include/exclude\nFor the include-exclude string,\
     first character can be +(include) or -(exclude).\n If include mode is used only mentioned backends are used and in exclude\
     mode only mentioned backends are ignored.",argv[0]);
    return 0;
  }

  device_limit = atoi(argv[1]);
  timeout = atoi(argv[2]);
  strncpy(includes,argv[3],sizeof(includes));
  
  if(strlen(argv[3])>=sizeof(includes))
    includes[sizeof(includes)-1]='\0';
  
  if(timeout<1)
  {
    timeout = DEFAULT_TIMEOUT_LIMIT;
  }
  if(device_limit<1)
  {
    device_limit = -1;
  }
  signal_listeners();
  if((server_bin=getenv("CUPS_SERVERBIN"))==NULL){
    server_bin = DEFAULT_SERVERBIN;
  }
  
  snprintf(dirname,sizeof(dirname),"%s/backend",server_bin);
  if((dir = cupsDirOpen(dirname))==NULL){
    fprintf(stderr,"ERROR: Unable to open backend"
                    "directory(%s): %s\n",dirname,strerror(errno));
    exit(1);
  }
  if(strlen(includes))
  {
    if(includes[0]=='-') isInclude = 0;
    int k=0;
    char *cj = includes+1;
    for(;*cj;cj++)
    {
      if(*cj==',') {
        if(k==0) continue;
        if(isInclude){
          inc_backends[inc_length][k++]=0;
          inc_length++;
        }
        else{
          exc_backends[exc_length][k++]=0;
          exc_length++;
        }
        k=0;
      }
      else{
        if(isInclude) inc_backends[inc_length][k++]=*cj;
        else exc_backends[exc_length][k++]=*cj;
      }
    }
    if(k)
    if(isInclude){
      inc_backends[inc_length][k++]=0;
      inc_length++;
    }
    else {
      exc_backends[exc_length][k++]=0;
      exc_length++;
    }
  }
  while((dent = cupsDirRead(dir))!=NULL)
  {
    if (!S_ISREG(dent->fileinfo.st_mode) ||
        !isalnum(dent->filename[0] & 255) ||
        (dent->fileinfo.st_mode & (S_IRUSR | S_IXUSR)) != (S_IRUSR | S_IXUSR))
      continue;
    
    int invalid = 0;
    if(inc_length){
      invalid = 1;
      for(int i=0;i<inc_length;i++)
      {
        if(!strncasecmp(inc_backends[i],dent->filename,strlen(inc_backends[i])))
          invalid = 0;
      }
    }
    else{
      for(int i=0;i<exc_length;i++)
      {
        if(!strncasecmp(exc_backends[i],dent->filename,strlen(exc_backends[i])))
        {
          invalid = 1;
        }
      }
    }
    if(!invalid)
      start_backend(dent->filename, !(dent->fileinfo.st_mode & (S_IWGRP | S_IRWXO)));
  }
  cupsDirClose(dir);
  
  end_time = get_current_time() + timeout;

  while(active_backends>0 &&(current_time=get_current_time())<end_time)
  {
    timeout =(int)(1000*(end_time-current_time));
    if (poll(backend_fds, (nfds_t)num_backends, timeout) > 0)
    {
      for (i = 0; i < num_backends; i++)
        if (backend_fds[i].revents && backends[i].pipe)
	{
	  cups_file_t *bpipe = backends[i].pipe;
					/* Copy of pipe for backend... */

    while(get_device(backends+i)){
      backend_fds[i].fd = 0;
      backend_fds[i].events = 0;
    }
  }
    }
  }

}

static double				/* O - Time in seconds */
get_current_time(void)
{
  struct timeval	curtime;	/* Current time */
  gettimeofday(&curtime, NULL);
  return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
}

static int get_device(backend_t *backend)
{
  char line[2048];
  if(cupsFileGets(backend->pipe,line,sizeof(line))){
    fprintf(stdout,"%s\n",line);
    return 1;
  }
  cupsFileClose(backend->pipe);
  backend->pipe =NULL;
  return 0;
}

static int				/* O - 0 on success, -1 on error */
start_backend(const char *name,		/* I - Backend to run */
              int        root)		/* I - Run as root? */
{
  const char		*server_bin;	/* CUPS_SERVERBIN environment variable */
  char			program[1024];	/* Full path to backend */
  backend_t	*backend;	/* Current backend */
  char			*argv[2];	/* Command-line arguments */


  if (num_backends >= MAX_BACKENDS)
  {
    fprintf(stderr, "ERROR: Too many backends (%d)!\n", num_backends);
    return (-1);
  }

  if ((server_bin = getenv("CUPS_SERVERBIN")) == NULL)
    server_bin = DEFAULT_SERVERBIN;

  snprintf(program, sizeof(program), "%s/backend/%s", server_bin, name);
  
  // if (_cupsFileCheck(program, _CUPS_FILE_CHECK_PROGRAM, !geteuid(),
  //                    _cupsFileCheckFilter, NULL))
  //   return (-1);
  if(!fileCheck(program))
    return -1;

  backend = backends + num_backends;

  argv[0] = (char *)name;
  argv[1] = NULL;

  if ((backend->pipe = cupsdPipeCommand(&(backend->pid), program, argv,
                                        root ? 0 : normal_user)) == NULL)
  {
    fprintf(stderr, "ERROR: [deviced] Unable to execute \"%s\" - %s\n",
            program, strerror(errno));
    return (-1);
  }

 /*
  * Fill in the rest of the backend information...
  */

  fprintf(stderr, "DEBUG: [deviced] Started backend %s (PID %d)\n",
          program, backend->pid);

  backend_fds[num_backends].fd     = cupsFileNumber(backend->pipe);
  backend_fds[num_backends].events = POLLIN;

  backend->name   = strdup(name);
  backend->status = 0;
  backend->count  = 0;

  active_backends ++;
  num_backends ++;

  return (0);
}


static void sigchld_handler(int sig,siginfo_t *siginfo, void*context)
{
    int			i;		/* Looping var */
  int			status;		/* Exit status of child */
  int			pid;		/* Process ID of child */
  backend_t	*backend;	/* Current backend */
  const char		*name;		/* Name of process */


 /*
  * Reset the dead_children flag...
  */
  pid = siginfo->si_pid;
  dead_children = 0;

 /*
  * Collect the exit status of some children...
  */
  status = siginfo->si_status;

    for (i = num_backends, backend = backends; i > 0; i --, backend ++)
      if (backend->pid == pid)
        break;

    if (i > 0)
    {
      name            = backend->name;
      backend->pid    = 0;
      backend->status = status;

      active_backends --;
    }
    else
      name = "Unknown";

    if (status)
    {
      if (WIFEXITED(status))
	fprintf(stderr,
	        "ERROR: [deviced] PID %d (%s) stopped with status %d!\n",
		pid, name, WEXITSTATUS(status));
      else
	fprintf(stderr,
	        "ERROR: [deviced] PID %d (%s) crashed on signal %d!\n",
		pid, name, WTERMSIG(status));
    }
    else
      fprintf(stderr,
              "DEBUG: [deviced] PID %d (%s) exited with no errors.\n",
	      pid, name);
}