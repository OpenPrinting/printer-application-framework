#
# IPP tools makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# include ../Makedefs
LIBCUPS		=	libcups.so.2
LIBCUPSIMAGE	=	libcupsimage.so.2
LIBCUPSOBJS	=	$(COREOBJS) $(DRIVEROBJS)
LIBCUPSSTATIC	=	libcups.a
LIBGSSAPI	=	-L/usr/lib/x86_64-linux-gnu/mit-krb5 -Wl,-Bsymbolic-functions -Wl,-z,relro -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err
LIBHEADERS	=	$(COREHEADERS) $(DRIVERHEADERS)
LIBHEADERSPRIV	=	$(COREHEADERSPRIV) $(DRIVERHEADERSPRIV)
LIBMALLOC	=	
LIBPAPER	=	
LIBUSB		=	-lusb-1.0
LIBWRAP		=	
LIBZ		=	-lz
LD_CC		= 	gcc -g
LD_CXX		= 	g++ -g
#
# Install static libraries?
#

INSTALLSTATIC	=	

#
# IPP backend aliases...
#

IPPALIASES	=	http https ipps


#
# ippeveprinter commands...
#

IPPEVECOMMANDS	=	ippevepcl ippeveps

CODE_SIGN	=	/bin/true
CODE_SIGN_IDENTITY = -

#
# Program options...
#
# ARCHFLAGS     Defines the default architecture build options.
# OPTIM         Defines the common compiler optimization/debugging options
#               for all architectures.
# OPTIONS       Defines other compile-time options (currently only -DDEBUG
#               for extra debug info)
#

ALL_CFLAGS	=	-I.. -D_CUPS_SOURCE $(CFLAGS) \
			$(SSLFLAGS) -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT \
			$(ONDEMANDFLAGS) $(OPTIONS)
ALL_CXXFLAGS	=	-I.. -D_CUPS_SOURCE $(CXXFLAGS) \
			$(SSLFLAGS) -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT \
			$(ONDEMANDFLAGS) $(OPTIONS)
ALL_DSOFLAGS	=	-L/home/dj/cupsi/lib/   $(DSOFLAGS) $(OPTIM)
ALL_LDFLAGS	=	-L/home/dj/cupsi/lib/ $(LDFLAGS)  \
			-fPIE -pie $(OPTIM)
ARCHFLAGS	=	
ARFLAGS		=	crvs
BACKLIBS	=	
BUILDDIRS	=	test filter backend berkeley cgi-bin monitor notifier ppdc scheduler systemv conf data desktop locale man doc examples templates
CFLAGS		=  -I.. -isystem /usr/include/mit-krb5  -isystem /usr/include/mit-krb5  -I/usr/include/libusb-1.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -DDBUS_API_SUBJECT_TO_CHANGE -D_FORTIFY_SOURCE=2 -D_REENTRANT
COMMONLIBS	=	-lpthread -lm -lcrypt   -lz
CXXFLAGS	=	-I.. -isystem /usr/include/mit-krb5   -D_FORTIFY_SOURCE=2
CXXLIBS		=	
DBUS_NOTIFIER	=	dbus
DBUS_NOTIFIERLIBS =	-ldbus-1
DNSSD_BACKEND	=	dnssd
DSOFLAGS	=	 -Wl,-soname,`basename $@` -shared
DSOLIBS		=	$(LIBZ) $(COMMONLIBS)
DNSSDLIBS	=	-lavahi-common -lavahi-client
IPPFIND_BIN	=	ippfind
IPPFIND_MAN	=	ippfind.$(MAN1EXT)
LDFLAGS		=	
LINKCUPS	=	-lcups $(LIBGSSAPI) $(DNSSDLIBS) $(SSLLIBS) $(LIBZ)
LINKCUPSIMAGE	=	-lcupsimage
LINKCUPSSTATIC	=  -l:libcups.so $(LIBGSSAPI) $(DNSSDLIBS) $(SSLLIBS) $(LIBZ)
LIBS		=	$(LINKCUPS) $(COMMONLIBS)
ONDEMANDFLAGS	=	
ONDEMANDLIBS	=	-lsystemd
OPTIM		=	-fPIC -Os -g -fstack-protector -D_GNU_SOURCE
OPTIONS		=	-Werror -Wno-error=deprecated-declarations -Wall -Wno-format-y2k -Wunused -Wno-unused-result -Wsign-conversion -Wno-format-truncation -Wno-tautological-compare
PAMLIBS		=	-lpam -ldl
SERVERLIBS	=	  -ldbus-1
SSLFLAGS	=	-I/usr/include/p11-kit-1
SSLLIBS		=	-lgnutls
UNITTESTS	=	



OBJS		=	\
			ippevepcl.o \
			ippeveprinter.o \
			ippeveps.o \
			ippfind.o \
			ipptool.o
IPPTOOLS	=	\
			ippeveprinter \
			$(IPPFIND_BIN) \
			ipptool
TARGETS		=	\
			$(IPPEVECOMMANDS) \
			$(IPPTOOLS) \
			$(LOCALTARGET)


#
# Make all targets...
#

all:		$(TARGETS)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(IPPTOOLS) $(IPPEVECOMMANDS) $(OBJS)
	$(RM) ippeveprinter-static ippfind-static ipptool-static


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:
	echo Installing IPP tools in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	for file in $(IPPTOOLS); do \
		$(INSTALL_BIN) $$file $(BINDIR); \
	done
	echo Installing printer commands in $(SERVERBIN)/command...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/command
	for file in $(IPPEVECOMMANDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/command; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(IPPTOOLS) $(IPPEVECOMMANDS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done; \
	fi


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Unnstall all targets...
#

uninstall:
	echo Uninstalling IPP tools from $(BINDIR)...
	for file in $(IPPTOOLS); do \
		$(RM) $(BINDIR)/$$file; \
	done
	-$(RMDIR) $(BINDIR)
	echo Uninstalling print commands from $(SERVERBIN)/command...
	for file in $(IPPEVECOMMANDS); do \
		$(RM) $(SERVERBIN)/command/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/ippeveprinter


#
# Local programs (not built when cross-compiling...)
#

local:	ippeveprinter-static ipptool-static


#
# ippeveprinter
#

ippeveprinter:	ippeveprinter.o 
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveprinter.o $(DNSSDLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippeveprinter-static
#

ippeveprinter-static:	ippeveprinter.o 
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveprinter.o /home/dj/cupsi/lib/libcups.so $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

# ippeveprinter.o: ippeveprinter.c ../cups/cups-private.h ../cups/string-private.h ../cups/config.h ../cups/versioning.h \
#   ../cups/array-private.h ../cups/array.h ../cups/ipp-private.h ../cups/cups.h ../cups/file.h ../cups/ipp.h ../cups/http.h \
#   ../cups/language.h ../cups/pwg.h ../cups/http-private.h ../cups/language-private.h ../cups/transcode.h ../cups/pwg-private.h \
#   ../cups/thread-private.h ../cups/ppd-private.h ../cups/ppd.h ../cups/raster.h printer-png.h 
# 	$(LD_CC) $(ALL_CFLAGS) -c ippeveprinter.c

# ippeveprinter-static: ippeveprinter.o 
# 	$(LD_CC) $(ALL_LDFLAGS) $(LINKCUPS) -o ippeveprinter ippeveprinter.o 


#
# ippevepcl
#

ippevepcl:	ippevepcl.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippevepcl.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippeveps
#

ippeveps:	ippeveps.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveps.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippfind
#

ippfind:	ippfind.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippfind.o $(DNSSDLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippfind-static
#

ippfind-static:	ippfind.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippfind.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ipptool
#

ipptool:	ipptool.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ipptool.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ipptool-static
#

ipptool-static:	ipptool.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ipptool.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Generate the header containing the data for printer.png...
#

pngheader:
	echo "Generating printer-png.h from printer.png..."
	echo "static const unsigned char printer_png[] =" >printer-png.h
	echo "{" >>printer-png.h
	od -t x1 printer.png | cut -b12- | awk '{printf("  "); for (i = 1; i <= NF; i ++) printf("0x%s,", $$i); print "";}' >>printer-png.h
	echo "};" >>printer-png.h


#
# Dependencies...
#

include Dependencies
