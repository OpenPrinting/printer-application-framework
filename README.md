# Printer Applications Framework

## What is a Printer Application?

Printer Application is a daemon which detects the supported printers and advertizes those printers on the localhost as an IPP Everywhere printer. Printer Applications are basically an extension of printer driver packages.

## Why do we need Printer Applications?

The current printing system relies on printer driver packages to add support for non-driverless printers. Canonical is now moving towards sandboxed or snapped package distribution system. In a sandboxed cups package, we cannot modify directory contents once it is snapped. Our system is no more modular. We cannot choose which printer driver package to install. Printer Applications address this problem of modularity and gives us same freedom as in the case of printer drivers.

## How a Printer Application Work?

Working of a Printer Application can be divided into 3 parts:

1. **Device Detection**
2. **PPD Searching**
3. **IPP Eveprinter Manager**
4. **IPP Eveprinter Command**

### Device Detection

File: `server/detection.c, server/server.c and server/server.h`

We have to detect two types of devices - local printers(connected using usb,tty and parallel ports) and network printers(non driverless network printers). For local printers we are using udev to detect any hardware change on usb, tty and parallel ports. 
Function ```monitor_devices``` detects local printers. Whenever we detect any change on usb we increment corresponding value in ```pending_signals``` array. ```enum child_signal``` describes an event and its corresponding index in the pending_signals array. 
Function ```monitor_avahi_devices``` detects network printers and corresponding value is incremented in the ```pending_signals``` array.

```monitor_devices``` and ```monitor_avahi_devices``` are run as seperate threads. The ```pending_signals``` array is processed in the main thread. The ```server::main``` function every 10 seconds check if any value of ```pending_signals``` is non-zero.
If any value is non-zero then ```get_devices``` function is called with the corresponding index.

```get_devices``` function generates include/exclude scheme based on the index number and call the ```deviced``` utility. This utility is based on the ```cups-deviced``` utility but is simpler. The ```deviced``` utility gives us all the available printers(filtered using the include/exclude). This list is stored in ```temp_devices``` array and it is then compared with the ```con_devices``` array. The ```con_devices``` array maintains all the printers which have corresponding ```ippeveprinter``` active on the localhost. If we have to add a printer, PPD is searched. If we have to remove a printer, IPP eveprinter manager is called.

### PPD Searching

