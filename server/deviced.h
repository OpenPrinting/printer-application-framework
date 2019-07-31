/*
 *  Printer Application Framework.
 * 
 *  deviced utility invokes all the available backends and get device list.
 *  It is largely based on the cups-deviced utility of cups package.
 *
 *  Copyright 2019 by Dheeraj.
 *
 *  Licensed under Apache License v2.0.  See the file "LICENSE" for more
 *  information.
 */

#include "server.h"
#include <cups/file.h>
#include <cups/dir.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <error.h>
#include <wait.h>

#define DEFAULT_TIMEOUT_LIMIT 1000
#define DEFAULT_SERVERBIN "/usr/lib/cups/"