#include "log.h"

/*
 *  getLock(char*) - Get lock of a file
 *  Returns - 
 *  0   - Some other process have locked this file.
 *  +ve - File is locked.
 *  -1  - Permission error.
 */
int _getLock(char *filename)
{
    char lockname[PATH_MAX];
    snprintf(lockname,sizeof(lockname),"%s.lock",filename);
    int fd = open(lockname,O_CREAT|O_WRONLY|O_EXCL,
    S_IRUSR|S_IWUSR);
    if(fd<0){
        if(errno==EEXIST){
            return 0;
        }
        return -1;
    }
    return fd;
}

/*
 *  waitForLock(char*) - Wait until we get lock on the file.
 *  Returns - 
 *  -1  - Error.
 *  0   - Timeout.
 *  +ve   - FD of lock file.
 */
int _waitForLock(char *filename)
{
    int loop=0;
    int timeout = DEFAULT_TIMEOUT;
    int num_tries = DEFAULT_NUM_CHECKS;
    if(getenv("PAF_NUM_CHECKS")){
        num_tries = atoi(getenv("PAF_NUM_CHECKS"));
    }
    if(getenv("PAF_TIMEOUT")){
        timeout = atoi(getenv("PAF_TIMEOUT"));
    }
    int lock_fd = 0;
    while(loop<num_tries)
    {
        loop++;
        log_fd = _getLock(filename);
        
        if(log_fd>0)
            return log_fd;
        else if(log_fd<0)
            return -1;
        
        struct timespec ts;
        ts.tv_sec = timeout/1000;
        ts.tv_nsec = (timeout%1000)*1000000;
        nanosleep(&ts,NULL);
    }
    return 0;
}

int _releaseLock(char *filename, int fd)
{
    char lockname[PATH_MAX];
    snprintf(lockname,sizeof(lockname),"%s.lock",filename);
    if(fd<0){
        return -1;
    }
    close(fd);
    unlink(lockname);
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
int initialize_log()
{
    if(log_initialized) return 0;
    char *tmpdir = strdup((getenv("SNAP_COMMON")?getenv("SNAP_COMMON"):"/var/tmp/"));
    snprintf(logfile,sizeof(logfile),"%s/logs.txt",tmpdir);
    if(getenv("DEBUG_LEVEL"))
    {
        int temp_level = atoi(getenv("DEBUG_LEVEL"));
        if(temp_level>3||temp_level<0){
            temp_level = 1; //Simply ignore
        }
        log_level = temp_level;
    }
    if(log_level){
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

int debug_printf(const char *format, ...)
{
    va_list arg;
    int res =0 ;
    int message_level=0;
    if(!strncmp(format,"ERROR:",6))
        message_level = 1;
    else if(!strncmp(format,"DEBUG:",6))
        message_level = 2;
    else if(!strncmp(format,"DEBUG2:",7))
        message_level = 3;
    if(message_level<=log_level)
    {
        va_start(arg,format);
        res = _debug_log(format,arg);
        va_end(arg);
    }
    return res;
}

static int _debug_log(const char *format, va_list arg)
{
    int res;
    if(initialize_log())
        return -1;
    int log_fd = _waitForLock(logfile);
    fprintf(stderr,"Format: %s\n",format);
    if(log_fd<=0){
        fprintf(stderr,"ERROR: Unable to get lock on logfile!\n");
        return -1;
    }
    FILE* logs = fopen(logfile,"a");
    if(logs==NULL)
    {
        fprintf(stderr,"ERROR: Unable to open logfile!\n");
        return -1;
    }
    res = vfprintf(logs,format,arg);
    fclose(logs);
    _releaseLock(logfile,log_fd);
    return res;
}

// int logFromPipe()

// int main()
// {
//     debug_printf("HELLO WORLD\n");
//     debug_printf("ERROR: HELLO THERE\n");
//     debug_printf("DEBUG: What's up?\n");
//     debug_printf("DEBUG2: HELLO %d %s %s %s %s %s %d\n",1, "Dheerajjjjjjjjjjjjjjjjjjjjjj",
//     "Dheeraj","Dheeraj","Dheeraj","Dheeraj",1000);
//     return 0;
// }