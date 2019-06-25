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
exec_prefix = /usr
ALL_CFLAGS	=	 -D_CUPS_SOURCE $(CFLAGS) \
			$(SSLFLAGS) -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT \
			$(ONDEMANDFLAGS) $(OPTIONS)
ALL_CXXFLAGS	=	 -D_CUPS_SOURCE $(CXXFLAGS) \
			$(SSLFLAGS) -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT \
			$(ONDEMANDFLAGS) $(OPTIONS)
ALL_DSOFLAGS	=   $(DSOFLAGS) $(OPTIM)
ALL_LDFLAGS	=	  $(LDFLAGS)  \
			-fPIE -pie $(OPTIM)
ARCHFLAGS	=	
ARFLAGS		=	crvs
BACKLIBS	=	
BUILDDIRS	=	test filter backend berkeley cgi-bin monitor notifier ppdc scheduler systemv conf data desktop locale man doc examples templates
CFLAGS		=	-isystem /usr/include/mit-krb5  -isystem /usr/include/mit-krb5  -I/usr/include/libusb-1.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -DDBUS_API_SUBJECT_TO_CHANGE -D_FORTIFY_SOURCE=2 -D_REENTRANT
COMMONLIBS	=	-lpthread -lm -lcrypt   -lz
CXXFLAGS	=	-isystem /usr/include/mit-krb5   -D_FORTIFY_SOURCE=2
CXXLIBS		=	
DBUS_NOTIFIER	=	dbus
DBUS_NOTIFIERLIBS =	-ldbus-1
DNSSD_BACKEND	=	dnssd
DSOFLAGS	=	 -Wl,-soname,`basename $@` -shared
DSOLIBS		=	$(LIBZ) $(COMMONLIBS)
DNSSDLIBS	=	-lavahi-common -lavahi-client
IPPFIND_BIN	=	ippfind
IPPFIND_MAN	=	ippfind.$(MAN1EXT)
LDFLAGS		=	-Wl,-rpath,${exec_prefix}/lib
LINKCUPS	=	-lcups $(LIBGSSAPI) $(DNSSDLIBS) $(SSLLIBS) $(LIBZ)
LINKCUPSIMAGE	=	-lcupsimage
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

# ALL_CFLAGS = $(CFLAGS)
# CFLAGS = `cups-config --cflags`
# ALL_LDFLAGS = $(LDFLAGS)
# LDFLAGS = `cups-config --ldflags`
# ALL_LIBS = $(LIBS)
# LIBS = `cups-config --libs`

util.o: util.c
	$(LD_CXX) $(ALL_CFLAGS) -c util.c

server.o:	server.c framework-config.h
	$(LD_CC) $(ALL_CFLAGS) -c server.c 

server:		server.o util.o 
	$(LD_CC) $(ALL_LDFLAGS) -o server server.o util.o $(LIBS) -ludev
#	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

deviced.o: deviced.c 
	$(LD_CC) $(ALL_CFLAGS) -c deviced.c 

deviced:	deviced.o util.o
	$(LD_CC) $(ALL_LDFLAGS) -o deviced deviced.o util.o $(LIBS)
#	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

ippprint.o: ippprint.c
	$(LD_CC) $(CFLAGS) -c ippprint.c

ippprint:	ippprint.o
	$(LD_CC) $(ALL_LDFLAGS)  -o ippprint ippprint.o  $(LIBS)

mime_type.o: mime_type.c
	$(LD_CC) $(CFLAGS) -c mime_type.c

mime_type: mime_type.o
	$(LD_CC) $(ALL_LDFLAGS) -o mime_type mime_type.o util.o $(LIBS)