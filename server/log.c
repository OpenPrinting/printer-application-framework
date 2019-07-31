#include "log.h"

/*
 *  getLock(char*) - Get lock of a file
 *  Returns - 
 *  0   - Some other process have locked this file.
 *  +ve - File is locked.
 *  -1  - Permission error.
 *  Sometimes, a process might exit without releasing the lock,
 *  this can have a very bad effect on the entire system. So, we
 *  store pid of locking process in the lock file. If the process
 *  has exited then we need to delete this lock file.
 */
static int _getLock(char *filename,int trynum)
{
    char lockname[PATH_MAX];
    snprintf(lockname,sizeof(lockname),"%s.lock",filename);
    int fd = open(lockname,O_CREAT|O_WRONLY|O_EXCL,
    S_IRUSR|S_IWUSR);
    if(fd<0){
        if(errno==EEXIST){
            if(trynum%50==0)
            {
                fd = open(lockname,O_RDONLY);
                if(fd<0){
                    return 0;
                }
                else{
                    char pstring[10];
                    int sz = read(fd,pstring,sizeof(pstring));
                    // fprintf(stderr,"READ: %d\n",sz);
                    if(sz>=sizeof(pstring))
                        pstring[sizeof(pstring)-1]=0;
                    int pid = atoi(pstring);
                    int kres = kill(pid,0);
                    if(kres<0){
                        if(errno==ESRCH){
                            int res = remove(lockname);
                        }
                    }
                }
            }
            return 0;
        }
        return -1;
    }
    char pidstring[10];
    snprintf(pidstring,sizeof(pidstring),"%d",getpid());
    write(fd,pidstring,sizeof(pidstring));
    return fd;
}

/*
 *  waitForLock(char*) - Wait until we get lock on the file.
 *  Returns - 
 *  -1  - Error.
 *  0   - Timeout.
 *  +ve   - FD of lock file.
 */
static int _waitForLock(char *filename)
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
        log_fd = _getLock(filename,loop);
        
        if(log_fd>0)
            return log_fd;
        else if(log_fd<0)
            return -1;
        
        struct timespec ts;
        ts.tv_sec = timeout/1000;
        ts.tv_nsec = (timeout%1000)*1000000;
        nanosleep(&ts,NULL);
        loop++;
    }
    return 0;
}

/*
 *  _releaseLock(filename,fd) - Release lock on file-filename
 *  Returns-
 *  0 - Success
 *  -1 - fd is negative.
 */
static int _releaseLock(char *filename, int fd)
{
    char lockname[PATH_MAX];
    snprintf(lockname,sizeof(lockname),"%s.lock",filename);
    if(fd<0){
        return -1;
    }
    close(fd);
    unlink(lockname);
    return 0;
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

/*
 * debug_printf(char *,...) - Print to log file.
 * returns -
 * -1   - Error.
 * else Number of bytes written.
 */
int debug_printf(char *format, ...)
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
static int _debug_log(char *format, va_list arg)
{
    int res;
    char timestring[128];
    char format2[3096];
    gettime(timestring,sizeof(timestring));
    snprintf(format2,sizeof(format2),"[%s] %s",timestring,format);
    if(initialize_log())
        return -1;
    int log_fd = _waitForLock(logfile);
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
    res = vfprintf(logs,format2,arg);
    fclose(logs);
    _releaseLock(logfile,log_fd);
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

// int main()
// {
//     // debug_printf("HELLO WORLD\n");
//     // debug_printf("ERROR: HELLO THERE\n");
//     // debug_printf("DEBUG: What's up?\n");
//     // debug_printf("DEBUG2: HELLO %d %s %s %s %s %s %d\n",1, "Dheerajjjjjjjjjjjjjjjjjjjjjj",
//     // "Dheeraj","Dheeraj","Dheeraj","Dheeraj",1000);
//     cups_file_t* file = cupsFileOpen("/var/snap/hplip-printer-application/common/logs.txt","r");
//     if(file==NULL)
//     {
//         fprintf(stderr,"Unable to open file!\n");
//         return 0;
//     }
//     logFromFile(file);
//     return 0;
// }