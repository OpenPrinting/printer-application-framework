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
#include "framework-config.h"
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
#define DEVICED_TIM "500"
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

void add_devices(cups_array_t *con, cups_array_t *temp);
void remove_devices(cups_array_t *con,cups_array_t *temp);
int remove_ppd(char* ppd);
int start_ippeveprinter(device_t *dev,int port);
int getport(int arr_size);
#include "detection.c"