# Printer Applications Framework

## What is a Printer Application?

Printer Application is a daemon which detects the supported printers and advertizes those printers on the localhost as an IPP Everywhere printer. Printer Applications are an extension of printer driver packages.

## Why do we need Printer Applications?

The current printing system relies on printer driver packages to add support for non-driverless printers. Canonical is now moving towards sandboxed or snapped package distribution system. In a sandboxed cups package, we cannot modify directory contents once it is snapped. Our system is no more modular. We cannot choose which printer driver package to install. Printer Applications address this problem of modularity and give us the same freedom as in the case of printer drivers.

## How a Printer Application Works?

Working of a Printer Application can be divided into 4 parts:

1. **Device Detection**
2. **PPD Searching**
3. **IPP Eveprinter Manager**
4. **IPP Eveprinter Command**

### Device Detection

File: `server/detection.c, server/server.c and server/server.h`

We have to detect two types of devices - local printers(connected using usb,tty and parallel ports) and network printers(non driverless network printers). For local printers, we are using udev to detect any hardware change on usb, tty and parallel ports.

Function ```monitor_devices``` detects local printers. Whenever we detect any change on usb we increment corresponding value in ```pending_signals``` array. ```enum child_signal``` describes an event and its corresponding index in the pending_signals array.
Function ```monitor_avahi_devices``` detects network printers and the corresponding value is incremented in the ```pending_signals``` array.

```monitor_devices``` and ```monitor_avahi_devices``` are run as seperate threads. The ```pending_signals``` array is processed in the main thread. The ```server:: main``` function every 10 seconds check if any value of ```pending_signals``` is non-zero.
If any value is non-zero then ```get_devices``` function is called with the corresponding index.

```get_devices``` function generates include/exclude scheme based on the index number and call the ```deviced``` utility. This utility is based on the ```cups-deviced``` utility but is simpler. The ```deviced``` utility gives us all the available printers(filtered using the include/exclude). This list is stored in ```temp_devices``` array and it is then compared with the ```con_devices``` array. The ```con_devices``` array maintains all the printers which have corresponding ```ippeveprinter``` active on the localhost. If we have to add a printer, PPD is searched. If we have to remove a printer, IPP eveprinter manager is called.

### PPD Searching

To search for PPD file, we are using CUPS's ```cups-driverd``` utility with a minor modification(to search for PPD files in the snap package instead of LSB folders). This modified ```cups-driverd``` utility is imported from [dheeraj135:ippsample](https://github.com/dheeraj135/ippsample). This ippsample repository have support for PPD files, have modified cups-driverd code and have support for accepting only pwg-raster docformats. Currently, we are using device's manufacturer-make and device's ieee 1284 ID string for searching a suitable PPD file. If we don't find any PPD file for a printer, then we cannot support this printer and it is ignored. If we find PPD for the printer, the PPD file is copied to ```/var/snap/$SNAP_NAME/common/ppd/``` folder. 

### IPP Eveprinter Manager

This part contains two functions - ```start_ippeveprinter``` and ```kill_ippeveprinter```. If we find a new printer in the ```temp_devices``` array which is not present in the ```con_devices``` array and we have PPD file for this printer then ```start_ippeveprinter``` function is called. If we have a printer in ```con_devices``` which is not present in ```temp_devices``` and the backend of this printer was invoked by the ```deviced``` utility then we have to remove this printer and ```kill_ippeveprinter``` function is called.

```start_ippeveprinter``` function first calls the ```get_port``` function to find a free port between the range 8000-9000. Please note that this function is prone to race condition. I was not able to find a function which can do this operation atomically. This port is used when invoking the ippeveprinter utility from the [dheeraj135:ippsample](https://github.com/dheeraj135/ippsample) repository. For each printer in ```con_devices``` we maintain the process id of this invoked ```ippeveprinter```.

```kill_ippeveprinter``` function sends **SIGINT** signal to the process id of the ippeveprinter to be killed.

### IPP Eveprinter Command

Files: `ippprint.c and mime_type.c`

Whenever a print job is submitted to the ippeveprinter, it calls the ```ippprint``` command. This ippprint command is responsible for applying the filter chain and sending the print job to the backend. First, we have to determine the filter chain. ```mime_type.c``` have code for this finding the filter chain. We read all the `.types` file and maintain all available formats in the `aval_types` array. This way we assign an index to each type. Next we initialize `mime_database` with the `aval_types` array. `mime_database->filter_graph` maintains an adjacency list of all available conversion. Next, we read all the `.convs` files and populate the `mime_database->filter_graph` adjacency list. Please note that we are not doing any kind of check to check whether a particular filter is available or not. This check will be added in future revisions.

Now, whenever we get a print job, we initialize `mime_database` as described above. Please note that, whenever we get a print job, the PPD file of the printer is also taken into consideration to use the cupsFilter and cupsFilter2 lines. So, native printer docformat is also added to the `aval_types` array. Now, we know the print job's document format and we know the destination format(printer's native docformat). We use Dijkstra to find the lowest weight path or you can say lowest weight filter chain.  This filter chain is stored in the `filter_chain` array.

Next, we generate full paths of these filters. When generating the full path, we make sure that filter is executable and permissions are correct. The filters with full names are stored in the `filterfullname` array. Please note that null filters(-) are ignored when generating the full paths. 

Next, we apply the filter chain. A series of pipes are created, environment variables `OUTFORMAT` is set. The final file is stored as `/var/snap/$SNAP_NAME/common/printjob.XXXXXX`, the last 6 X are set by the`mkstemp` function.

This file is used to invoke the `print_job` function, the `print_job` function in turn send the job to the backend and the backend is responsible for communicating with the printer and printing the job.

## How to use this framework?

Head on to the [Printer Application Snap repository](https://github.com/dheeraj135/Printer-Application-Snaps). The master branch contains the base `snap/local/snapcraft.yaml.in` file for using the framework. You can generate `snap/snapcraft.yaml` by running the `./configure` script. Currently, it just copies the file but it can(and will be) modified to allow changing the snapcraft.yaml file using the command line options and environment variables.

You need to add the part(s) to install the printer driver package on top of the framework. First please check different branches of [Printer Application Snap repository](https://github.com/dheeraj135/Printer-Application-Snaps) to see examples of different printer application snaps.

Whenever you are writing snap for your application, please make sure of the following things:

- All the run time dependencies are met using the `stage-package` or by adding new parts.
- When **building** the printer driver package, use cups directories as provided by `cups-config`(or the standard locations like CUPS_SERVERBIN=`/usr/lib/cups/`).
- When installing the printer driver package, your package should **not** ignore `DESTDIR` directive.
- If your printer driver package uses some symlinks, please make sure they are relative and don't point to absolute paths. If they point to absolute paths, please make sure these absolute paths point to correct location after they are installed. If you have any problem with the symlinks, please check the hplip snap.
- Your driver package should use `CUPS_*` environment variables whenever provided. 
- If your driver package needs someplace to store data, please use the path provided by the `SNAP_COMMON` environment variable.
- Please note that if your package installs some file in `/some/path/to/file`, after the snap is installed the file will be placed at `$SNAP/some/path/to/file`.
- If your printer driver package uses a language other than C/C++, you may have to provide appropriate environment variable so that it can locate required files and libraries.
- The filters will be located at `$SNAP/usr/lib/cups/filters`, similarly CUPS_SERVERROOT is `$SNAP/etc/cups`.

If your program is accessing a location outside the snap, it can be corrected in any of the following ways:

- Write a patch and apply it using the `override-build` property.
- Provide some environment variable, which your printer driver package understands.(FOOMATICDB in case of foomatic).
