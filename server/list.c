#include "server.h"

void initialize()
{
  if(getenv("SNAP"))
  {
    snap = strdup(getenv("SNAP"));
  }
  else snap =strdup("");
  con_devices = cupsArrayNew((cups_array_func_t)compare_devices,NULL);
  temp_devices = cupsArrayNew((cups_array_func_t)compare_devices,NULL);
}

int device_list()
{
    const char  *serverbin; // ServerBin
    char        program[2048];     // Full Path to program
    char        *argv[7];
    char        *env[7];
    char        name[32],reques_id[16],limit[16],
                timeout[16],user_id[16],options[1024];
    char        serverdir[1024],serverroot[1024],datadir[1024];
    process_t   *process;
    int         process_pid,status;
    char        includes[4096];
    cups_file_t *errlog;

    if((process = calloc(1,sizeof(process_t)))==NULL)
    {
      debug_printf("ERROR: Ran Out of Memory!\n");
      return (-1);
    }

    cupsArrayClear(temp_devices);
    cupsArrayClear(con_devices);
    
    strcpy(includes,"-");
    strcpy(reques_id,DEVICED_REQ);
    strcpy(limit,DEVICED_LIM);
    strcpy(timeout,DEVICED_TIM);
    strcpy(user_id,DEVICED_USE);
    strcpy(options,DEVICED_OPT);
    strcpy(name,"deviced");

    snprintf(program,sizeof(program),"%s/%s/%s",snap,BINDIR,name);
    snprintf(serverdir,sizeof(serverdir),"%s/%s",snap,SERVERBIN);
    snprintf(serverroot,sizeof(serverroot),"%s/etc/cups",snap);
    snprintf(datadir,sizeof(datadir),"%s/usr/share/cups",snap);

    setenv("CUPS_SERVERBIN",serverdir,1);
    setenv("CUPS_DATADIR",datadir,1);
    setenv("CUPS_SERVERROOT",serverroot,1);

    argv[0] = (char*) name;
    argv[1] = (char*) limit;
    argv[2] = (char*) timeout;
    argv[3] = (char*) includes;
    argv[4] = NULL;
    
    if((process->pipe = cupsdPipeCommand2(&(process->pid),program,argv,&errlog,
                            0))==NULL)
    {
        debug_printf("ERROR: Unable to execute deviced!\n");
        cupsFileClose(errlog);
        free(process);
        return (-1);
    }
    pthread_t logThread;
    logFromFile2(&logThread,errlog);
    if((process_pid = waitpid(process->pid,&status,0))>0)
    {
        while(!parse_line(process));
        pthread_join(logThread,NULL);
    }
    else{
      fprintf(stdout,"Failed to collect! PID ERROR! %d %s\n",process->pid,strerror(errno));
      return errno;
    }
    
    device_t *dev = cupsArrayFirst(temp_devices);
    for (;dev;dev=cupsArrayNext(temp_devices))
    {
        device_t* newDev = deviceCopy(dev);
        cupsArrayAdd(con_devices,newDev);
    }
    return 0;
}

int ppd_list()
{
  const char *serverbin;
  char program[2048];
  char *argv[6];
  char name[16],operation[8],request_id[4],limit[5],options[1024];
  char ppd_uri[128];
  char ppd_name[1024];  //full ppd path
  char escp_model[256];
  char *envp[6];
  char datadir[1024],serverdir[1024],cachedir[1024];
  char line[4096];
  cups_file_t *errlog;
  process_t *process;
  int        process_pid,status;
  if((process = calloc(1,sizeof(process_t)))==NULL)
  {
    debug_printf("ERROR: Ran Out of Memory!\n");
    return (-1);
  }
  strcpy(name,"cups-driverd");
  strcpy(operation,"list");
  strcpy(request_id,"0");
  strcpy(limit,"0");
  strcpy(options,"");
// snprintf(options,sizeof(options),"ppd-make-and-model=\'%s\' ppd-device-id=\'%s\'",make_and_model,device_id);
  
  snprintf(datadir,sizeof(datadir),"%s/%s",snap,DATADIR);
  snprintf(serverdir,sizeof(serverdir),"%s/%s",snap,SERVERBIN);
  snprintf(cachedir,sizeof(cachedir),"%s",tmpdir);

  setenv("CUPS_DATADIR",datadir,1);
  setenv("CUPS_SERVERBIN",serverdir,1);
  setenv("CUPS_CACHEDIR",cachedir,1);

  snprintf(program,sizeof(program),"%s/bin/%s",snap,name);

  argv[0] = (char*) name;
  argv[1] = (char*) operation;
  argv[2] = (char*) request_id;
  argv[3] = (char*) limit;
  argv[4] = (char*) options;
  argv[5] = NULL;

  debug_printf("DEBUG: Executing cups-driverd at %s\n",program);
  if((process->pipe = cupsdPipeCommand2(&(process->pid),program,
                        argv,&errlog,0))==NULL)
  {
    debug_printf("ERROR: Unable to execute!\n");
    cupsFileClose(errlog);
    free(process);
    return (-1);
  }
  pthread_t logThread;
  logFromFile2(&logThread,errlog);
  if((process_pid = waitpid(process->pid,&status,0))>0)
  {
      if(WIFEXITED(status))
      {
          while (cupsFileGets(process->pipe, line, sizeof(line))){
              printf("%s\n",line);
          }
      }
      pthread_join(logThread,NULL);
  }
  return 0;
}

int print_devices()
{
    if(device_list()){
        return 1;
    }
    device_t *dev = cupsArrayFirst(con_devices);
    for(;dev;dev=cupsArrayNext(con_devices))
    {
        printf("\"%s\" \"%s\" \"%s\"\n",dev->device_uri,dev->device_make_and_model,dev->device_id);
    }
    return 0;
}

int print_ppd_list()
{
    if(ppd_list()){
        return 1;
    }
    return 0;
}
void usage(char *arg)
{
  printf("Usage: %s -(p/d)\n"
   "-p: Print PPDs\n"
   "-d: Print Available devices\n",arg);
}
int main(int argc, char *argv[])
{
    initialize();
    int device=0,ppd=0;
    if(argc!=2)
    {
      usage(argv[0]);
      return -1;
    }
    for(int i=1;i<argc;i++)
    {
      if(strlen(argv[i])==1){
        usage(argv[0]);
        return -1;
      }
      if(argv[i][0]!='-'){
        usage(argv[0]);
        return -1;
      }
      switch(argv[i][1]){
        case 'p': ppd=1;
                  break;
        case 'd': device=1;break;
        default: printf("Invalid Argument\n");
                  usage(argv[0]);
                  return -1;
      }
    }
    if(ppd){
      print_ppd_list();
    }
    if(device){
      print_devices();
    }
    return 0;
}