#include <cups/cups.h>
#include <cups/array.h>
#include <cups/dir.h>
#include <cups/file.h>
#include <cups/ppd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <config.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_TYPE_LEN 64
#define MAX_TYPES 1000
#define INFINITY 10000
#define MAX_PIPES 10
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

int get_ppd_filter_chain(char* user_src,char* user_dest,char *ppdname,cups_array_t **arr);
filter_t* filterCopy(filter_t *t);