# Printer-Applications-Framework

This framework aims to provide a systematic method to create snaps of printer driver packages to use with cups.

## Installation

I recommend installing this framework in a custom directory rather than in root directories.

1. Download the source code and ``cd`` to that direcotry.
2. mkdir build
3. ```export pre="`pwd`/build"```
4. ./configure --prefix=$pre
5. make
6. make install

Now to use this framework with hplip:
Make sure to set the ```pre``` environment variable.
1. Cd to hplip directory.
2. ```./configure --with-drvdir=$pre/share/cups/drv/hp/ --with-mimedir=$pre/share/cups/mime/ --with-hpppddir=$pre/share/cups/ppd/hp/ --with-cupsfilterdir=$pre/lib/cups/filter/ --with-cupsbackenddir=$pre/lib/cups/backend/ --prefix=$pre/```
3. make
4. make install

## How to use

1. cd $pre/bin
2. sudo ./server 

