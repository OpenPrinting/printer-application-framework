/*
 *  Printer Application Framework.
 * 
 *  This is the helper code of the ippprint program. This code is responsible
 *  for determining the filter chain.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#include "ippprint.h"
#include "util.h"

static database_t *mime_database;
static cups_array_t* aval_convs;
static cups_array_t* aval_types;
static cups_array_t* aval_types_name;

static filter_t* filterNew() {
  filter_t *filter = calloc(1, sizeof(filter_t));  
  return filter;
}

static type_t* typeNew() {
  type_t *type = calloc(1, sizeof(type_t));
  return type;
}

static type_t* typeCopy(type_t* t) {
  type_t* ret = typeNew();
  ret->index = t->index;
  ret->typename = strdup(t->typename);
  return ret;
}

filter_t* filterCopy(filter_t *t) {
  filter_t* ret = filterNew();
  ret->filter = strdup(t->filter);
  ret->src = typeCopy(t->src);
  ret->dest = typeCopy(t->dest);
  return ret;
}

static int getIndex(type_t* type) {
  /*fprintf(stdout, "Searching: %s %d\n", type->typename,
          strlen(type->typename));*/
  type_t* poi = cupsArrayFind(aval_types_name, type);
  if (poi) {
    /*fprintf(stdout, "Found: %s at %d\n", poi->typename, poi->index);*/
    return poi->index;
  } else
    return -1;
}

static int compare_types_name(type_t* t1, type_t* t2) {
  return strcasecmp(t1->typename, t2->typename);
}

static int compare_types(type_t* t1, type_t *t2) {
  return t1->index-t2->index;
}

static int compare_filters(filter_t *f1, filter_t *f2) {
  int st = 0;
  if ((st = compare_types(f1->src, f2->src)))
    return st;
  else if (st = compare_types(f1->dest, f2->dest))
    return st;
  else
    return strcasecmp(f1->filter, f2->filter);
}

/*
 * addFilter() - Add a filter with given specifications to the conversion table.
 * 
 * Returns:
 *  0 - Success
 *  !0 - Error
 */
static int addFilter(char* src_typename, char *dest_typename,
		     char *filter_name, int cost) {
  int res;
  filter_t* filter = filterNew();

  filter->cost = cost;

  type_t* src = typeNew();
  src->typename = strdup(src_typename);
  src->index = getIndex(src);

  type_t* dest = typeNew();
  dest->typename = strdup(dest_typename);
  dest->index = getIndex(dest);

  filter->src = src;
  filter->dest = dest;
  filter->filter = strdup(filter_name);
  /*fprintf(stdout, "Done ALL-1! %d %d\n", src->index, dest->index);*/
  if (src->index < 0 || dest->index < 0)       /* Invalid Typename */
    return -1;

  cups_array_t** temp_arr = mime_database->filter_graph;
  temp_arr += src->index;

  cupsArrayAdd(*temp_arr, filter);

  return 0;
}

/*
 * getFilter() - Parse the filter line and call addFilter.
 * 
 * Returns-
 * 0 - Success
 * !0 - Error
 */
static int getfilter(char *line, int len) {
  int cost;
  char temp[3][128];
  int res = 0;
  res = sscanf(line,"%s\t%s\t%d\t%s", temp[0], temp[1],
	       &(cost), temp[2]);
  if (res != 4)
    return -1;
  res = addFilter(temp[0], temp[1], temp[2], cost);
  return res;
}

/*
 * addType() - Create and add typename
 */
static int addType(char *typename) {
  type_t* type = calloc(1, sizeof(type));
  type_t* type1 = calloc(1, sizeof(type));

  if (type1 == NULL || type == NULL) {
    debug_printf("Unable to allocate memory!\n");
    return -1;
  }
  type->typename = strdup(typename);
  type1->typename = strdup(typename);
  if (cupsArrayFind(aval_types_name, type) == NULL) {
    type1->index = cupsArrayCount(aval_types);
    type->index = type1->index;
    cupsArrayAdd(aval_types_name, type);
    cupsArrayAdd(aval_types, type1);
  } else {
    free(type);
    free(type1);
  }
}

static void gettype(char* line, int len) {
  char temp[128];
  sscanf(line, "%s", temp);
  addType(temp);
}

static void read_file(char* fname,int conv)
{
  cups_file_t* in_file = cupsFileOpen(fname, "r");

  if (in_file == NULL)
    debug_printf("Unable to read!\n");
  char line[2048];
  while (cupsFileGets(in_file, line, sizeof(line))) {
    if (line == NULL)
      break;
    int len = strlen(line);
    if (!isalpha(line[0]))
      continue;
    if (line && len >= 2 && line[0] != '#') {
      if(conv)
        getfilter(line, sizeof(line));
      else
        gettype(line, sizeof(line));
    }
  }
  cupsFileClose(in_file);
}

static void readDir(char* dirname, int read_convo) {
  cups_dir_t *dir = cupsDirOpen(dirname);
  cups_dentry_t* dentry;
  if (dir == NULL)
    return;
  while ((dentry = cupsDirRead(dir))) {
    char *fname = strdup(dentry->filename);

    char desname[2048];
    snprintf(desname, sizeof(desname), "%s/%s",
	     dirname, fname);
    fname = strrev(fname);
    int p;
    if (S_ISDIR(dentry->fileinfo.st_mode)) {
      readDir(desname,read_convo);
      continue;
    }
    if ((p = strncmp(fname, "svnoc.", 6)) == 0) {
      if (read_convo)
	read_file(desname, 1);
    } else if ((p = strncmp(fname, "sepyt.", 6)) == 0) {
      if (!read_convo)
	read_file(desname, 0);
    }
    free(fname);
  }
  cupsDirClose(dir);
}

static void load_convs(int read_convo) {
  char mime_dir[1024];
  char *datadir, *snap;

  datadir = getenv("CUPS_DATADIR");
  if (datadir == NULL) {
    if ((snap = getenv("SNAP")) == NULL)
      snap = "";
    datadir = "/usr/share/cups";
  } else
    snap = "";

  snprintf(mime_dir, sizeof(mime_dir), "%s%s/mime/", snap, datadir);
  debug_printf("DEBUG: Reading directory %s\n", mime_dir);
  readDir(mime_dir, read_convo);
}

static void createDatabase() {
  int num_types = cupsArrayCount(aval_types);
  mime_database = calloc(1, sizeof(database_t));
  if (mime_database == NULL) {
    fprintf(stdout, "Unable to allocate memory!\n");
    exit(0);
  }
  mime_database->num_types = num_types;
  mime_database->filter_graph = calloc(num_types, sizeof(cups_array_t*));
  cups_array_t **temp;
  temp = mime_database->filter_graph;
  for(int i=0; i < num_types; i++) {
    *temp = cupsArrayNew((cups_array_func_t)compare_filters, NULL);
    temp++;
  }
}

static int initialize_filter_chain() {
  aval_convs = cupsArrayNew((cups_array_func_t)compare_filters, NULL);
  aval_types_name = cupsArrayNew((cups_array_func_t)compare_types_name, NULL);
  aval_types = cupsArrayNew((cups_array_func_t)compare_types, NULL);

#if 0
  for (type_t *t = cupsArrayFirst(aval_types); t;
       t = cupsArrayNext(aval_types))
    fprintf(stdout, "%d %s\n", t->index, t->typename);
#endif
}

static int minDistanceVertex(int *distance, int *inTree) {
  int min = INFINITY, min_index;
  int v = 0;
  int num_types = mime_database->num_types;
  /*fprintf(stdout, "Distance of %d is %d\n", src_index,
    distance[src_index]);*/
  for (; v < num_types; v++) {
    if ((!inTree[v]) && (distance[v] < min)) {
      /*fprintf(stdout, "Distance of %d is %d\n", v, distance[v]);*/
      min = distance[v];
      min_index = v;
    }
  }
  return min_index;
}

static filter_t* getFilter(int src_index, int dest_index) {
  cups_array_t* arr = *(mime_database->filter_graph + src_index);
  for (filter_t* t = cupsArrayFirst(arr); t; t = cupsArrayNext(arr)) {
    if (t->dest->index == dest_index) {
      filter_t *ret = filterCopy(t);
      return ret;
    }
  }
  return NULL;
}

static int getMinCostConversion(int src_index,int dest_index,cups_array_t *arr)
{
  debug_printf("DEBUG: Finding conversion : %d -> %d\n",src_index,dest_index);
  if(src_index==dest_index)
  {
    arr=NULL;
    return 0;
  }
  int i = 0, j = 0;         /* Iterators */
  int distance[MAX_TYPES];
  int inTree[MAX_TYPES];    /* Flag */
  int parent[MAX_TYPES];

  int num_types = mime_database->num_types;
  
  for (; i < num_types; i++) {
    parent[i] = -1;
    distance[i] = INFINITY;
    inTree[i] = 0;
  }
  distance[src_index] = 0;
  /*fprintf(stdout, "Distance of %d is %d\n", src_index,
	  distance[src_index]);*/
  for (i = 0; i < num_types - 1; i++) {
    int u = minDistanceVertex(distance, inTree);
    /*fprintf(stdout, "Min Vertex: %d\n", u);*/
    inTree[u] = 1;
    cups_array_t* arr = *(mime_database->filter_graph + u);
    for (filter_t* k = cupsArrayFirst(arr); k; k = cupsArrayNext(arr)) {
      int index_v = k->dest->index;
      if (!inTree[index_v] && distance[u] + k->cost < distance[index_v]) {
        parent[index_v] = u;
        distance[index_v] = distance[u] + k->cost;
      }
    }
  }
#if 0
  for (int i = 0; i < num_types; i++)
    fprintf(stdout, "%d %d\n", i, distance[i]);
#endif
  if (parent[dest_index] < 0) {
    arr = NULL;
    return -1;
  }
  /* fprintf(stdout, "YO\n");*/
  int curr = dest_index;
  int par = parent[curr];
  while (curr >= 0 && par >= 0) {
    filter_t* filter = getFilter(par, curr);
    if (filter == NULL)
      return -1;
    cupsArrayAdd(arr, filter);
    curr = par;
    par = parent[curr];
  }
  return 0;
}

static int get_filter_chain(char* user_src, char* user_dest,
			    cups_array_t **arr) {
  cups_array_t *temp;
  type_t *src, *dest;
  src = typeNew();
  dest = typeNew();
  src->typename = calloc(MAX_TYPE_LEN, sizeof(char));
  dest->typename = calloc(MAX_TYPE_LEN, sizeof(char));
  strncpy(src->typename, user_src, MAX_TYPE_LEN);
  strncpy(dest->typename, user_dest, MAX_TYPE_LEN);
  int src_index = getIndex(src);
  int dest_index = getIndex(dest);
  if (src_index < 0 || dest_index < 0) {
    *arr = NULL;
    debug_printf("ERROR: Not found in types! %d %d\n", src_index, dest_index);
    return -1;
  }

  *arr = cupsArrayNew(NULL,NULL);
  temp = cupsArrayNew(NULL,NULL);
  
  int ret = getMinCostConversion(src_index,dest_index,temp);
  if(ret<0) {
    debug_printf("ERROR: Unable to find a filter chain!\n");
    return -1;
  }
  int num_fil = cupsArrayCount(temp);
  for (int i = num_fil - 1; i >= 0; i--) {
    filter_t *s = filterCopy(cupsArrayIndex(temp, i));
    cupsArrayAdd(*arr, s);
  }
  cupsArrayDelete(temp);
  return 0;
}

int get_ppd_filter_chain(char* user_src, char *user_dest, char *ppdname,
			 cups_array_t **arr) {
  char ventorType[128];
  strcpy(ventorType, "printer/");
  initialize_filter_chain();
  load_convs(0);            //Load Types
  ppd_file_t* ppd = ppdOpenFile(ppdname);
  if (ppd == NULL) {
    debug_printf("ERROR: Unable to open PPD!\n");
    /*return -1;*/
  }
  char **filters;
  if (ppd) {
    filters = ppd->filters;
    for (int i = 0; i < ppd->num_filters; i++) {
      char src[128], filter[128];
      int cost;
      sscanf(*filters, "%s %d %s", src, &cost, filter);
      addType(src);
    }
    addType(ventorType);
  }
  createDatabase();
  load_convs(1);
  if (ppd) {
    filters = ppd->filters;
    for (int i = 0; i < ppd->num_filters; i++) {
      char src[128], filter[128];
      int cost;
      sscanf(*filters, "%s %d %s", src, &cost, filter);
      addFilter(src, ventorType, filter, cost);
    }
  }
  if (ppd) {
    if (ppd->num_filters == 0)
      return get_filter_chain(user_src, user_dest, arr);
    else
      return get_filter_chain(user_src, ventorType, arr);
  } else if (user_dest)
    return get_filter_chain(user_src, user_dest, arr);

  return -1;

#if 0
  if (user_dest)
    get_filter_chain(user_src, user_dest, arr);
  else if (ppd) 
    get_filter_chain(user_src, ventorType, arr);
  else return -1;
#endif
}

#if 0
void setup() {
  initialize_filter_chain();
  load_convs(0);
  createDatabase();
  load_convs(1);
}

int main() {
  cups_array_t* filter_chain;
  /*setup();*/
  /*int res = get_filter_chain("image/jpeg", "application/vnd.cups-pdf",
			     &filter_chain);*/
  int res = get_ppd_filter_chain("application/pdf",
				 "/home/dj/Desktop/HP-OfficeJet-Pro-6960.ppd",
				 &filter_chain);
  fprintf(stdout, "RES:%d\n", res);
  for (filter_t* f = cupsArrayFirst(filter_chain); f;
       f = cupsArrayNext(filter_chain))
    fprintf(stdout, "FILTER: %s(%s->%s)\n", f->filter, f->src->typename,
	    f->dest->typename);
}
#endif
