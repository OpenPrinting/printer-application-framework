#########################################################################
# Makefile for printer applications framework project.					#
# 																		#
# Copyright 2019 by Dheeraj.											#
#																		#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more	#
# information. 															#
#########################################################################

bin_PROGRAMS = server deviced ippprint libusb #mime_type
sbin_PROGRAMS = 

# if HAVE_AVAHI
# LIB_AVAHI = -lavahi-common -lavahi-client
# else
# LIB_AVAHI = 
# endif

LIB_UDEV = -ludev
CUPS_TEMP = -lgss -lpthread

# CUPS_LIBS=$(CUPS_STATIC)

AM_CFLAGS = -I.. $(CUPS_CFLAGS)
AM_LDFLAGS = $(CUPS_LDFLAGS)

deviced_SOURCES = deviced.c util.c util.h
deviced_LDADD = $(CUPS_LIBS) $(LIB_AVAHI) $(CUPS_TEMP)
deviced_LDFLAGS = 

ippprint_SOURCES = ippprint.c mime_type.c util.c ippprint.h
ippprint_LDADD = $(CUPS_LIBS) $(LIB_AVAHI) $(CUPS_TEMP)
ippprint_LDFLAGS = 

# mime_type_SOURCES = mime_type.c util.c util.h ippprint.h
# mime_type_LDADD = $(LIB_CUPS)

server_SOURCES = server.c util.c server.h util.h
server_LDADD = $(CUPS_LIBS) $(LIB_AVAHI) $(CUPS_TEMP) $(LIB_UDEV) 
server_LDFLAGS = 

libusb_SOURCES = libusb.c
libusb_LDADD = -lusb-1.0 -ludev -lpthread

DIRECTORIES = $(tmpdir)/ppd \
	$(datadir)/drv $(datadir)/mime/ $(datadir)/model
	
DIR2 =	$(cachedir) $(serverbin)/backend $(serverbin)/driver $(serverbin)/filter $(bindir)

# install: $(install) installdirss

# install-data-local:
# 	$(MKDIR_P) $(DIRECTORIES) $(DIR2)
# 	$(RM) $(datadir)/ppdc
# 	$(LN_S) $(CUPS_DATADIR)/ppdc $(datadir)/ppdc
# 	$(RM) $(datadir)/mime/cups-mime
# 	$(LN_S) $(CUPS_DATADIR)/mime $(datadir)/mime/cups-mime
# 	$(RM) $(serverbin)/filter/cups-filter
# 	$(LN_S) $(CUPS_SERVERBIN)/filter $(serverbin)/filter/cups-filter

# install-exec-local:
# 	chown -R root:root $(datadir)
# 	chmod -R 777 $(tmpdir)
# 	chmod 755 $(serverbin)/backend/*