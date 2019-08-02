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
#include "log.h"

/*
 *  getLock(char*) - Get lock of a file
 *  Returns - 
 *  0 - Success
 *  -1  - Error.
 */
static int _getLock(cups_file_t *file,int block)
{
    return cupsFileLock(file,block);
}

/*
 *  _releaseLock(filename,fd) - Release lock on file-filename
 *  Returns-
 *  0 - Success
 *  -1 - Error
 */
static int _releaseLock(cups_file_t *file)
{
    return cupsFileUnlock(file);
}

/*
 * Debug Levels-
 * 0 - Print Nothing
 * 1 - Print only "ERROR:" lines
 * 2 - Print everything above and "DEBUG:" lines
 * 3 - Print everything above and "DEBUG2:" lines
 * 
 * Returns-
 *  -1  - Error
 *  0   - Success
 */
static int initialize_log()
{
    if(log_initialized) return 0;
    char *tmpdir = strdup((getenv("SNAP_COMMON")?getenv("SNAP_COMMON"):"/var/tmp/"));
    char *logname = strdup((getenv("LOG_NAME")?getenv("LOG_NAME"):"logs.txt"));
    snprintf(logfile,sizeof(logfile),"%s/%s",tmpdir,logname);
    int temp_level;
    if(getenv("DEBUG_LEVEL"))
    {
        temp_level = atoi(getenv("DEBUG_LEVEL"));
    }
    else{
        temp_level = DEBUG_LEVEL;
    }
    if(temp_level>3||temp_level<0){
            temp_level = 1; //Simply ignore
        }
        log_level = temp_level;
    if(log_level>1){
        fprintf(stderr,"Initializing Debugging!\n");
    }
    int logfd = open(logfile,O_CREAT|O_WRONLY|O_EXCL,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(logfd<0)
    {
        if(errno!=EEXIST){
            fprintf(stderr,"ERROR: Unable to Initialize Debugging!\n");
            fprintf(stderr,"ERROR: Unable to open log file");
            return -1;
        }
    }
    else close(logfd);
    log_initialized =1;
    return 0;
}

/*
 * debug_printf(char *,...) - Print to log file.
 * returns -
 * -1   - Error.
 * else Number of bytes written.
 */
int debug_printf(char *format, ...)
{
    initialize_log();
    va_list arg;
    int res =0 ;
    char logline[3096];
    va_start(arg,format);
    vsnprintf(logline,sizeof(logline),format,arg);
    va_end(arg);
    int message_level=0;
    if(!strncmp(logline,"ERROR:",6))
        message_level = 1;
    else if(!strncmp(logline,"DEBUG:",6))
        message_level = 2;
    else if(!strncmp(logline,"DEBUG2:",7))
        message_level = 3;
    if(message_level<=log_level)
    {
        res = _debug_log(logline,sizeof(logline));
    }
    return res;
}

/*
 * gettime() - Get Date Time String.
 * Returns - Void
 */
static void gettime(char *timestring,int len)
{
    time_t rawtime = time(NULL);
    if(rawtime<0)
    {
        timestring="0-0-0";
        return;
    }
    struct tm *ptm = localtime(&rawtime);
    strftime(timestring,len,"%d-%b-%y %a %T %z ",ptm);
}

/*
 * _debug_log(char *,va_list) - Print to file
 * Returns -
 * -1   -   Error
 * else Number of bytes written
 */
static int _debug_log(char *logline,int len)
{
    int res;
    char timestring[128];
    char format2[3224];
    gettime(timestring,sizeof(timestring));
    snprintf(format2,sizeof(format2),"[%s] %s",timestring,logline);
    format2[sizeof(format2)-1]=0;
    cups_file_t* logs = cupsFileOpen(logfile,"a");
    if(logs==NULL)
    {
        fprintf(stderr,"ERROR: FILE NULL\n");
    }
    _getLock(logs,1);
    res= cupsFileWrite(logs,format2,strlen(format2));
    cupsFileClose(logs);
    _releaseLock(logs);
    return res;
}

/*
 * logFromFile() - Read lines from a cups file and write to log file.
 * Returns - 
 * -1 - Error
 * 0 - Success
 */
int logFromFile(cups_file_t *file)
{
    if(file==NULL) 
        return -1;
    char line[2048];
    int len;
    while(cupsFileGets(file,line,sizeof(line)))
    {
        len = strlen(line);
        if(len<sizeof(line))
        {
            line[len+1]=(char)0;
            line[len]='\n';
        }
        debug_printf(line);
    }
    return 0;
}

void * _logThread(void *t)
{
    cups_file_t *file = (cups_file_t*)t;
    if(file==NULL){
        pthread_exit((void*)NULL);
    }
    char line[2048];
    int len;
    while(cupsFileGets(file,line,sizeof(line)))
    {
        len = strlen(line);
        if(len<sizeof(line))
        {
            line[len+1]=(char)0;
            line[len]='\n';
        }
        debug_printf(line);
    }
    pthread_exit((void*)NULL);
}

void logFromFile2(pthread_t *process_logger, cups_file_t* file)
{
    pthread_create(process_logger,NULL,_logThread,(void*)file);
}
