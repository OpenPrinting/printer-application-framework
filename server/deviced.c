#include "util.h"
#include "deviced.h"
#include <cups/file.h>
#include <cups/dir.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <error.h>
#include <wait.h>

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
  cups_dir_t *dir;    // FD
  cups_dentry_t *dent;
  double end_time,current_time;

  device_limit = atoi(argv[1]);
  timeout = atoi(argv[2]);
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
  while((dent = cupsDirRead(dir))!=NULL)
  {
    if (!S_ISREG(dent->fileinfo.st_mode) ||
        !isalnum(dent->filename[0] & 255) ||
        (dent->fileinfo.st_mode & (S_IRUSR | S_IXUSR)) != (S_IRUSR | S_IXUSR))
      continue;
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

  backend = backends + num_backends;

  argv[0] = (char *)name;
  argv[1] = NULL;

  if ((backend->pipe = cupsdPipeCommand(&(backend->pid), program, argv,
                                        root ? 0 : normal_user)) == NULL)
  {
    fprintf(stderr, "ERROR: [cups-deviced] Unable to execute \"%s\" - %s\n",
            program, strerror(errno));
    return (-1);
  }

 /*
  * Fill in the rest of the backend information...
  */

  fprintf(stderr, "DEBUG: [cups-deviced] Started backend %s (PID %d)\n",
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
	        "ERROR: [cups-deviced] PID %d (%s) stopped with status %d!\n",
		pid, name, WEXITSTATUS(status));
      else
	fprintf(stderr,
	        "ERROR: [cups-deviced] PID %d (%s) crashed on signal %d!\n",
		pid, name, WTERMSIG(status));
    }
    else
      fprintf(stderr,
              "DEBUG: [cups-deviced] PID %d (%s) exited with no errors.\n",
	      pid, name);
}