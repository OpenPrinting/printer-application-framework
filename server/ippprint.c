
#include "ippprint.h"


extern char **environ;

FILE *sout,*serr;

void ini()
{
  char logs[2048];
  snprintf(logs,sizeof(logs),"%s/logs.txt",TMPDIR);
  sout = fopen(logs,"a");
  fprintf(sout,"*****************************************************\n");
}

int main()
{
    ini();
    char **s = environ;
    
    // for(;*s;){
    //     fprintf(sout,"%s\n",*s);
    //     s = (s+1);
    // }
    if(getenv("PPD")==NULL){
      exit(0);
    }
    char *ppdname = strdup(getenv("PPD"));
    ppd_file_t* ppd = ppdOpenFile(ppdname);
    if(ppd==NULL)
    {
      fprintf(stderr,"Unable to open PPD!\n");
      exit(1);
    }
    fprintf(sout,"Num Filters: %d\n",ppd->num_filters);
    char **fil = ppd->filters;
    for(int i=0;i<ppd->num_filters;i++)
    {
      fprintf(sout,"Filter: %s\n",*fil);
      fil = fil+1;
    }
    fprintf(sout,"*****************************************************\n");
    fclose(sout);
    return 0;
}