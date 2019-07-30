/*
 *  Printer Application Framework.
 * 
 *  ippprint script is executed by the ippeveprinter. This script is 
 *  responsible for applying the filter chain and sending the job to
 *  the backend.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#ifndef PAF_IPPPRINT_H

#define PAF_IPPPRINT_H 1

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
#include <pwd.h>
#include <errno.h>
#include "util.h"

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

FILE *sout,*serr;

int get_ppd_filter_chain(char* user_src,char* user_dest,char *ppdname,cups_array_t **arr);
filter_t* filterCopy(filter_t *t);

#endif