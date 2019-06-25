/*
 *  Printer Application Framework.
 * 
 *  This is the detection and PPD matcher of the Printer Application.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */
#include <config.h>
#include <cups/file.h>
#include <cups/array.h>
#include "util.h"
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>

#define MAX_DEVICES 1000
#define DEVICED_REQ "1"
#define DEVICED_LIM "100"
#define DEVICED_TIM "5"
#define DEVICED_USE "1"
#define DEVICED_OPT "\"\""

#define SUBSYSTEM "usb"
typedef struct
{
    char name[1024];
    int pid, status;
    cups_file_t *pipe;
} process_t;

typedef struct
{
    char device_class[128],
    device_info[128],
    device_uri[1024],
    device_location[128],
    device_make_and_model[512],
    device_id[128];
    char ppd[1024];
    int eve_pid;
} device_t;

enum child_signal{
    NO_SIGNAL,
    AVAHI_ADD,
    AVAHI_REMOVE,
    USB_ADD,
    USB_REMOVE
};
static device_t devices[MAX_DEVICES];
static cups_array_t *con_devices;
static cups_array_t *temp_devices;
static int compare_devices(device_t *d0,device_t *d1);

static int get_devices();
static int parse_line(process_t*);
static int		process_device(const char *device_class,
				   const char *device_make_and_model,
				   const char *device_info,
				   const char *device_uri,
				   const char *device_id,
				   const char *device_location);

static int get_ppd(char* ppd,int ppd_len,char *make_and_model,int make_len,
                    char *device_id, int dev_len);
int get_ppd_uri(char* ppd_uri,process_t* process);
int print_ppd(process_t* backend,cups_file_t* tempPPD);

int monitor_usb_devices(pid_t ppid);

void add_devices(cups_array_t *con, cups_array_t *temp);
void remove_devices(cups_array_t *con,cups_array_t *temp);
int remove_ppd(char* ppd);
int start_ippeveprinter(device_t *dev,int port);
int getport();
static int kill_ippeveprinter(pid_t pid);
#include "detection.c"