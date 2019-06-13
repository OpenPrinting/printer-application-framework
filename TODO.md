# Get PPD 
- Use limit = 1 and first of all test by installing hplip.

# Child Process
- For Child Process: https://stackoverflow.com/a/36945270

# For listening on udev and avahi - Look into it.

# ...

# CONFIGURE Command: 
```
export CPPFLAGS='-I/home/dj/cups/'

export LDFLAGS="-L/home/dj/cupsi/lib"

./configure --with-drvdir=$pre/share/drv/hp/ --with-mimedir=$pre/share/mime/ --with-hpppddir=$pre/share/ppd/hp/ --with-cupsfilterdir=$pre/filter/ --with-cupsbackenddir=$pre/backend/
```

