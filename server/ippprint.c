
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
    int res =cupsArrayAdd(*filter_fullname,out);
    // fprintf(stderr,"Add res: %d %s\n",res,out);
  }
}

static int createOptionArray(char *op)  /*O-*/
{
  
}

int main()
{
    ini();
    char **s = environ;
    int isPPD = 1,isOut=1;    
    // for(;*s;){
    //     fprintf(sout,"%s\n",*s);
    //     s = (s+1);
    // }
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
    // output_type=strdup("application/pdf");
    // content_type=strdup("application/postscript");
    if(isPPD) 
      ppdname = strdup(getenv("PPD"));
    content_type = strdup(getenv("CONTENT_TYPE"));
    if(isOut)
      output_type = strdup(getenv("OUTPUT_TYPE"));
    cups_array_t *filter_chain,*filterfullname;
    char *paths;
    filter_t *tempFilter;
    int res = get_ppd_filter_chain(content_type,output_type,ppdname,&filter_chain);
    // fprintf(stdout,"Res: %d\n",res);
    for(tempFilter=cupsArrayFirst(filter_chain);tempFilter;
      tempFilter=cupsArrayNext(filter_chain))
    {
      fprintf(sout,"Filter: %s\n",tempFilter->filter);
    }
    res = getFilterPaths(filter_chain,&filterfullname);
    // fprintf(stderr,"RESSS: %d %d\n",res,cupsArrayCount(filterfullname));
    for(paths=cupsArrayFirst(filterfullname);paths;
      paths=cupsArrayNext(filterfullname))
    {
      fprintf(sout,"Filter fn: %s\n",paths);
    }
    fprintf(sout,"*****************************************************\n");
    fclose(sout);
    return 0;
}