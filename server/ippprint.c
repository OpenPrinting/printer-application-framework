
#include "ippprint.h"


extern char **environ;

FILE *sout,*serr;

static void ini()
{
  char logs[2048];
  snprintf(logs,sizeof(logs),"%s/logs.txt",TMPDIR);
  fprintf(stdout,"%s\n",logs);
  sout = fopen(logs,"a");
  if(sout==NULL)
  {
    fprintf(stderr,"UNABLE TO OPEN!\n");
  }
  fprintf(sout,"*****************************************************\n");
}
static int getFilterPath(char *in, char **out)
{
  *out = NULL;
  char path[2048];
  snprintf(path,sizeof(path),"%s/filter/%s",SERVERBIN,in);
  // fprintf(stderr,"Checking: %s\n",path);
  if(access(path,F_OK|X_OK)!=-1)
  {
    *out = strdup(path);
    return 0;
  }
  snprintf(path,sizeof(path),"%s/filter",SERVERBIN);
  cups_dir_t* dir = cupsDirOpen(path);
  cups_dentry_t* dent;
  while((dent=cupsDirRead(dir)))  // Check only upto one level
  {
    char *filename = dent->filename;
    if(S_ISDIR(dent->fileinfo.st_mode))
    {
      snprintf(path,sizeof(path),"%s/filter/%s/%s",SERVERBIN,filename,in);
      // fprintf(stderr,"Checking: %s\n",path);
      if(access(path,F_OK|X_OK)!=-1){
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

static int getFilterPaths(cups_array_t *filter_chain, cups_array_t **filter_fullname)
{
  *filter_fullname=cupsArrayNew(NULL,NULL);
  char *in,*out;
  filter_t *currFilter;
  for(currFilter=cupsArrayFirst(filter_chain);currFilter;currFilter=cupsArrayNext(filter_chain))
  {
    in = currFilter->filter;
    if(getFilterPath(in,&out)==-1)
    {
      return -1;
    }
    
    filter_t* temp=filterCopy(currFilter);
    free(temp->filter);
    temp->filter=out;
    int res =cupsArrayAdd(*filter_fullname,temp);
    // fprintf(stderr,"Add res: %d %s\n",res,out);
  }
}

static int createOptionArray(char *op)  /*O-*/
{
  sprintf(op,"h");
  return 0;
}

static pid_t executeCommand(int inPipe,int outPipe,filter_t *filter,int i)
{
  pid_t pid;
  char *filename=filter->filter;
  // fprintf(stderr,"Executing: %s\n",filename);
  // fprintf(sout,"Filenmae: %s\n",filename);
  if((pid=fork())<0)
  {
    return -1;
  }
  else if(pid==0)
  {
    dup2(inPipe,0);
    dup2(outPipe,1);
    close(inPipe);
    close(outPipe);
    char *argv[10];
    argv[0]=strdup(filename);
    argv[1]=strdup("1");// Job id
    argv[2]=strdup("1000");// User
    argv[3]=strdup("Test");//Title
    argv[4]=strdup("1");//Copies
    argv[5]=strdup("\"\"");//Copies
    argv[6]=NULL;
    char newpath[1024];
    setenv("CUPS_SERVERBIN",CUPS_SERVERBIN,1);
    setenv("OUTFORMAT",filter->dest->typename,1);
    execvp(*argv,argv);
    fflush(stderr);
    exit(0);
  }
  else{
  int status=0;
  close(inPipe);
  close(outPipe);
  return pid;
  }
  exit(0);
}

static int applyFilterChain(cups_array_t* filters,char *inputFile,char *finalFile)
{
  int numPipes = cupsArrayCount(filters);
  numPipes = 3;
  int pipes[2*MAX_PIPES];
  cups_array_t* children=cupsArrayNew(NULL,NULL);
  char outName[1024];
  snprintf(outName,sizeof(outName),"%s/printjobXXXXXX",TMPDIR);
  if(children==NULL)
  {
    fprintf(stderr,"Out of memory!\n");
    return -1;
  }
  for(int i=0;i<=numPipes;i++)
  {
    int re = pipe(pipes+2*i);
  }
  int inputFd = open(inputFile,O_RDONLY);
  int outputFd = mkstemp(outName);
  dup2(inputFd,pipes[0]);
  close(inputFd);
  
  dup2(outputFd,pipes[2*numPipes+1]);  // Replace fd of outfile.
  // fprintf(sout,"TMP FILE: %s\n",outName);

  for(int i=0;i<numPipes;i++)
  {
    pid_t *pd=calloc(1,sizeof(pid_t));
    *pd = executeCommand(pipes[2*i],pipes[2*i+3],((filter_t*)cupsArrayIndex(filters,i)),i);  //read and write  
    cupsArrayAdd(children,pd);
    fprintf(stderr,"Adding: %d\n",*pd);
  }
  pid_t pd;
  int status;
  int killall=0;
  while(pd=waitpid(-1,&status,0)>0){
    if(WIFEXITED(status))
    {
      int es = WEXITSTATUS(status);
      fprintf(stderr,"%d Exited with status %d\n",pd,es);
      if(es){
        killall=1;    // One filter failed. kill entire chain.
        break;
      }
    }
  }
  if(killall){
    pid_t *temPid;
    for(temPid=cupsArrayFirst(children);temPid;temPid=cupsArrayNext(children))
    {
      kill(*temPid,SIGTERM);
    }
    return -1;
  }
  return 0;
}

void testApplyFilterChain()
{
  cups_array_t* t = cupsArrayNew(NULL,NULL);
  cupsArrayAdd(t,"1");
  cupsArrayAdd(t,"1");
  cupsArrayAdd(t,"1");
  cupsArrayAdd(t,"1");
  char inputFile[1024];
  snprintf(inputFile,sizeof(inputFile),"%s/logs.txt",TMPDIR);
  // char *outFile;
  applyFilterChain(t,inputFile,NULL);
}

int main(int argc, char *argv[])
{
    ini();
    // printf("EXIT!\n");
    // testApplyFilterChain();
    // return 0;
    char **s = environ;
    int isPPD = 1,isOut=1;    
    // for(;*s;){
    //     fprintf(sout,"%s\n",*s);
    //     s = (s+1);
    // }
    // exit(0);
    if(argc!=2)
    {
      return -1;
    }
    
    char *inputFile=strdup(argv[1]);

    if(getenv("CONTENT_TYPE")==NULL){
      exit(0);
    }
    if(getenv("PPD")==NULL){
      isPPD = 0;
    }
    if(getenv("OUTPUT_TYPE")==NULL){
      isOut =0;
    }
    char *ppdname=NULL;
    char *output_type=NULL;
    char *content_type=NULL;
    // ppdname=strdup("/home/dj/Desktop/HP-OfficeJet-Pro-6960.ppd");
    // setenv("PPD",ppdname,1);
    // content_type=strdup("application/pdf");
    // content_type=strdup("application/postscript");
    if(isPPD) 
      ppdname = strdup(getenv("PPD"));
    content_type = strdup(getenv("CONTENT_TYPE"));
    if(isOut)
      output_type = strdup(getenv("OUTPUT_TYPE"));
    cups_array_t *filter_chain,*filterfullname;
    filter_t *paths;
    filter_t *tempFilter;
    fprintf(sout,"%s %s %s\n",content_type,output_type,ppdname);
    int res = get_ppd_filter_chain(content_type,output_type,ppdname,&filter_chain);
    fprintf(sout,"Res: %d\n",res);
    for(tempFilter=cupsArrayFirst(filter_chain);tempFilter;
      tempFilter=cupsArrayNext(filter_chain))
    {
      fprintf(sout,"Filter: %s\n",tempFilter->filter);
    }
    res = getFilterPaths(filter_chain,&filterfullname);
    for(paths=cupsArrayFirst(filterfullname);paths;
      paths=cupsArrayNext(filterfullname))
    {
      fprintf(sout,"Filter fn: %s\n",paths->filter);
    }
    char finalFile[1024];
    applyFilterChain(filterfullname,inputFile,finalFile);
    fprintf(sout,"*****************************************************\n");
    fclose(sout);
    return 0;
}