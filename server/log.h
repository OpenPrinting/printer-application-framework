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
#include <signal.h>

static int log_fd;
static int log_level =1;
static int log_initialized=0;

static char logfile[PATH_MAX];

/*
 *  Private Functions
 */
static int _getLock(char *filename,int trynum);
static int _waitForLock(char *filename);
static int _releaseLock(char *filename,int fd);
static int initialize_log();
static int _debug_log(char *format, va_list arg);

/*
 * Public Functions
 */
int debug_printf(char* format, ...);
int logFromFile(cups_file_t *file);

#endif