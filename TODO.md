# Get PPD 
- Use limit = 1 and first of all test by installing hplip.

# Child Process
- For Child Process: https://stackoverflow.com/a/36945270 [Done]

# For listening on udev and avahi - Look into it. [libudev is done.]

# ...

# CONFIGURE Command: 
```
 ./configure --with-drvdir=$pre/share/cups/drv/hp/ --with-mimedir=$pre/share/cups/mime/ --with-hpppddir=$pre/share/cups/ppd/hp/ --with-cupsfilterdir=$pre/lib/cups/filter/ --with-cupsbackenddir=$pre/lib/cups/backend/ --prefix=$pre/
```

# Set user-id when calling cups-deviced and cups-driverd

# We need to add support for different URI(backends) in ippeveprinter.

# Do we need to handle driverless???? I don't think so!!!!
# Whatever we are doing here is for printer drivers!

