/*
 *  Printer Application Framework.
 * 
 *  This file handles debug logging of the Framework.
 *  Important environment variables-
 *      DEBUG_LEVEL = 0,1,2,3 [3 is the highest level of logging].
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#ifndef PAF_LOG_H

#define PAF_LOG_H 1

#define DEFAULT_NUM_CHECKS 1000
#define DEFAULT_TIMEOUT 100 // 100 milliseconds

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <cups/file.h>
#include <sys/file.h>
#include <pthread.h>
#include <config.h>

static int log_fd;
static int log_level =1;
static int log_initialized=0;

static char logfile[PATH_MAX];

/*
 *  Private Functions
 */
static int _getLock(cups_file_t *file, int block);
static int _releaseLock(cups_file_t *file);
static int initialize_log();
static int _debug_log(char *logline,int len);

/*
 * Public Functions
 */
int debug_printf(char* format, ...);
int logFromFile(cups_file_t *file);
void logFromFile2(pthread_t *logThread,cups_file_t *file);

#endif