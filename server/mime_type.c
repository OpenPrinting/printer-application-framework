#include "ippprint.h"
#include <cups/array.h>
#include <cups/dir.h>
#include <cups/file.h>
#include <ctype.h>
#include "util.h"

typedef struct{
  char *typename;
  int index;
}type_t;

typedef struct{
  type_t *src,
    *dest;
  char *filter;
  int cost;
}filter_t;

typedef struct{
  int num_types;
  cups_array_t **filter_graph;
}database_t;

database_t *mime_database;
cups_array_t* aval_convs;
cups_array_t* aval_types;
cups_array_t* aval_types_name;

filter_t* filterNew()
{
  filter_t *filter=calloc(1,sizeof(filter_t));  
  return filter;
}

type_t* typeNew()
{
  type_t *type = calloc(1,sizeof(type_t));
  return type;
}

type_t* typeCopy(type_t* t)
{
  type_t* ret = typeNew();
  ret->index = t->index;
  ret->typename = strdup(t->typename);
  return ret;
}

filter_t* filterCopy(filter_t *t)
{
  filter_t* ret = filterNew();
  ret->filter = strdup(t->filter);
  ret->src = typeCopy(t->src);
  ret->dest = typeCopy(t->dest);
  return ret;
}


int getIndex(type_t* type)
{
  // fprintf(stdout,"Searching: %s %d\n",type->typename,strlen(type->typename));
  type_t* poi = cupsArrayFind(aval_types_name,type);
  if(poi){
    // fprintf(stdout,"Found: %s at %d\n",poi->typename,poi->index);
    return poi->index;
  }
  else return -1;
}

static int compare_types_name( type_t* t1,
              type_t* t2)
{
  return strcasecmp(t1->typename,t2->typename);
}

static int compare_types(type_t* t1,
              type_t *t2)
{
  return t1->index-t2->index;
}

static int compare_filters( filter_t *f1,
              filter_t *f2)
{
  int st =0;
  if((st=compare_types(f1->src,f2->src)))
    return st;
  else if(st=compare_types(f1->dest,f2->dest))
    return st;
  else return strcasecmp(f1->filter,f2->filter);
}

void getfilter(char *line,int len)
{
  filter_t* filter = filterNew();
  char temp[3][128];
  sscanf(line,"%s\t%s\t%d\t%s",temp[0],temp[1],
              &(filter->cost),temp[2]);
  // fprintf(stdout,"%s %s %d %s\n",temp[0],temp[1],
  //             (filter->cost),temp[2]);
  type_t* src = typeNew();
  src->typename = strdup(temp[0]);
  src->index = getIndex(src);
  // fprintf(stdout,"Done src\n");
  type_t* dest = typeNew();
  dest->typename = strdup(temp[1]);
  dest->index = getIndex(dest);
  // fprintf(stdout,"Done dest\n");
  filter->src = src;
  filter->dest = dest;
  filter->filter = strdup(temp[2]);
  // fprintf(stdout,"Done ALL-1! %d %d\n",src->index,dest->index);
  if(src->index<0||dest->index<0)
    return;
  cups_array_t** temp_arr = mime_database->filter_graph;
  temp_arr+=src->index;
  cupsArrayAdd(*temp_arr,filter);
  // fprintf(stdout,"Done ALL!\n");
}

void gettype(char* line,int len)
{
  type_t* type = calloc(1,sizeof(type));
  type_t* type1 = calloc(1,sizeof(type));
  if(type==NULL)
  {
    fprintf(stderr,"Unable to allocate memory!\n");
    exit(1);
  }
  char temp[128];
  sscanf(line,"%s",temp);
  // fprintf(stdout,"TYPE: %s\n",temp);
  type->typename = strdup(temp);
  type1->typename = strdup(temp);
  if(cupsArrayFind(aval_types_name,type)==NULL)
  {
    type1->index = cupsArrayCount(aval_types);
    type->index = type1->index;
    // fprintf(stdout,"Adding %s %d %d\n",type->typename,type->index,strlen(type->typename));
    //fprintf(stdout,"Adding %s %d\n",type1->typename,type1->index);
    cupsArrayAdd(aval_types_name,type);
    cupsArrayAdd(aval_types,type1);
  }
  else{
    free(type);
    free(type1);
  }
}

void read_file(char* fname,int conv)
{
  char fullname[2048];
  snprintf(fullname,sizeof(fullname),"%s/mime/%s",DATADIR,fname);
  cups_file_t* in_file = cupsFileOpen(fullname,"r");

  if(in_file==NULL){
      fprintf(stderr,"Unable to read!\n");
  }
  char line[2048];
  while(cupsFileGets(in_file,line,sizeof(line)))
  {
    if(line==NULL)
    {
      break;
    }
    int len = strlen(line);
    if(!isalpha(line[0])) continue;
    if(line&&len>=2&&line[0]!='#'){
      if(conv)
        getfilter(line,sizeof(line));
      else
        gettype(line,sizeof(line));
    }
  }
  cupsFileClose(in_file);
}

void load_convs(int read_convo)
{
    char mime_dir[1024];
    snprintf(mime_dir,sizeof(mime_dir),"%s/mime/",DATADIR);
    cups_dir_t *dir = cupsDirOpen(mime_dir);
    cups_dentry_t* dentry;
    while((dentry=cupsDirRead(dir)))
    {
        char *fname = strdup(dentry->filename);
        char *p;
        // fprintf(stdout,"%s\n",fname);

        if(p=strstr(fname,".convs"))
        {
          if(read_convo)
            read_file(fname,1);
        }
        else if(p=strstr(fname,".types"))
        {
          if(!read_convo)
            read_file(fname,0);
        }
        free(fname);
    }
    cupsDirClose(dir);
}

void createDatabase()
{
  int num_types = cupsArrayCount(aval_types);
  mime_database = calloc(1,sizeof(database_t));
  if(mime_database==NULL)
  {
    fprintf(stdout,"Unable to allocate memory!\n");
    exit(0);
  }
  mime_database->num_types = num_types;
  mime_database->filter_graph = calloc(num_types,sizeof(cups_array_t*));
  cups_array_t **temp;
  temp = mime_database->filter_graph;
  for(int i=0;i<num_types;i++)
  {
    *temp = cupsArrayNew((cups_array_func_t)compare_filters,NULL);
    temp++;
  }
}

int initialize_filter_chain()
{
  aval_convs = cupsArrayNew((cups_array_func_t)compare_filters,NULL);
  aval_types_name = cupsArrayNew((cups_array_func_t)compare_types_name,NULL);
  aval_types = cupsArrayNew((cups_array_func_t)compare_types,NULL);
  load_convs(0);
  createDatabase();
  // fprintf(stdout,"Size: %d\n",mime_database->num_types);
  load_convs(1);
  int num_types = mime_database->num_types;
  // for(type_t* t=cupsArrayFirst(aval_types);t;t= cupsArrayNext(aval_types))
  // {
  //   fprintf(stdout,"%d %s\n",t->index,t->typename);
  // }
}

int minDistanceVertex(int *distance,int *inTree)
{
  int min = INFINITY, min_index;
  int v = 0;
  int num_types = mime_database->num_types;
  // fprintf(stdout,"Distance of %d is %d\n",src_index,distance[src_index]);
  for(;v<num_types;v++)
  {
    if((!inTree[v])&&(distance[v]<min)){
      // fprintf(stdout,"Distance of %d is %d\n",v,distance[v]);
      min = distance[v];
      min_index = v;
    }
  }
  return min_index;
}

filter_t* getFilter(int src_index,int dest_index)
{
  cups_array_t* arr = *(mime_database->filter_graph+src_index);
  for(filter_t* t=cupsArrayFirst(arr);t;t=cupsArrayNext(arr))
  {
    if(t->dest->index==dest_index){
      filter_t *ret = filterCopy(t);
      return ret;
    }
  }
}

int dijkstra(int src_index,int dest_index,cups_array_t *arr)
{
  fprintf(stdout,"Starting Dijkstra: %d -> %d\n",src_index,dest_index);
  if(src_index==dest_index)
  {
    arr=NULL;
    return 0;
  }
  int i =0,j=0;         // Iterators
  int distance[MAX_TYPES];
  int inTree[MAX_TYPES];    //Flag
  int parent[MAX_TYPES];

  int num_types = mime_database->num_types;
  
  for(;i<num_types;i++){
    parent[i] = -1;
    distance[i] = INFINITY;
    inTree[i] = 0;
  }
  distance[src_index] = 0;
  // fprintf(stdout,"Distance of %d is %d\n",src_index,distance[src_index]);
  for(i=0;i<num_types-1;i++)
  {
    int u = minDistanceVertex(distance,inTree);
    // fprintf(stdout,"Min Vertex: %d\n",u);
    inTree[u] = 1;
    cups_array_t* arr = *(mime_database->filter_graph+u);
    for(filter_t* k=cupsArrayFirst(arr);k;k=cupsArrayNext(arr))
    {
      int index_v = k->dest->index;
      if(!inTree[index_v] && distance[u]+ k->cost < distance[index_v])
      {
        parent[index_v] = u;
        distance[index_v] = distance[u]+k->cost;
      }
    }
  }
  // for(int i=0;i<num_types;i++)
  // {
  //   fprintf(stdout,"%d %d\n",i,distance[i]);
  // }
  if(parent[dest_index]<0)
  {
    arr=NULL;
    return -1;
  }
  // fprintf(stdout,"YO\n");
  int curr = dest_index;
  int par = parent[curr];
  while(curr>=0&&par>=0)
  {
    filter_t* filter = getFilter(par,curr);
    cupsArrayAdd(arr,filter);
    curr = par;
    par = parent[curr];
  }
  return 0;
}
int get_filter_chain(char* user_src, char* user_dest,cups_array_t **arr)
{
  cups_array_t *temp;
  initialize_filter_chain();

  type_t *src,*dest;
  src = typeNew();
  dest = typeNew();
  src->typename = calloc(MAX_TYPE_LEN,sizeof(char));
  dest->typename = calloc(MAX_TYPE_LEN,sizeof(char));
  strncpy(src->typename,user_src,MAX_TYPE_LEN);
  strncpy(dest->typename,user_dest,MAX_TYPE_LEN);
  int src_index = getIndex(src);
  int dest_index = getIndex(dest);
  if(src_index<0||dest_index<0)
  {
    *arr = NULL;
    fprintf(stdout,"Not found in types! %d %d\n",src_index,dest_index);
    return -1;
  }

  *arr = cupsArrayNew(NULL,NULL);
  temp = cupsArrayNew(NULL,NULL);
  
  dijkstra(src_index,dest_index,temp);
  for(filter_t* t=cupsArrayFirst(temp);t;t=cupsArrayNext(temp))
  {
    filter_t *s = filterCopy(t);
    cupsArrayAdd(*arr,s);
  }
  cupsArrayDelete(temp);
}
int main()
{
  cups_array_t* filter_chain;
  int res = get_filter_chain("image/jpeg","application/vnd.cups-pdf",&filter_chain);
  fprintf(stdout,"RES:%d\n",res);
  for(filter_t* f = cupsArrayFirst(filter_chain);f;f=cupsArrayNext(filter_chain))
  {
    fprintf(stdout,"FILTER: %s(%s->%s)\n",f->filter,f->src->typename,f->dest->typename);
  }
}