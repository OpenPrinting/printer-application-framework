bin_PROGRAMS = server deviced ippprint mime_type
sbin_PROGRAMS = 

LIB_AVAHI = -lavahi-common -lavahi-client
#LIB_CUPS = -lcups
LIB_UDEV = -ludev

AM_CFLAGS = -I..

deviced_SOURCES = deviced.c util.c util.h
deviced_LDADD = $(LIB_CUPS)

ippprint_SOURCES = ippprint.c ippprint.h
ippprint_LDADD = $(LIB_CUPS)

mime_type_SOURCES = mime_type.c util.c util.h ippprint.h
mime_type_LDADD = $(LIB_CUPS)

server_SOURCES = server.c util.c server.h
server_LDADD = $(LIB_CUPS) $(LIB_AVAHI) $(LIB_UDEV)

DIRECTORIES = $(prefix)/var/cache $(prefix)/var/tmp/ppd \
	$(datadir)/drv $(datadir)/mime $(datadir)/model
	
DIR2 =	$(cachedir) $(serverbin)/backend $(serverbin)/driver $(serverbin)/filter \
	$(ppddir) $(bindir)

# install: $(install) installdirss

install-data-local:
	echo "Serverbin: " $(serverbin)
	$(MKDIR_P) $(DIRECTORIES) $(DIR2)
	$(RM) $(datadir)/ppdc
	$(LN_S) $(CUPS_DATADIR)/ppdc $(datadir)/ppdc
	chown -R root:root $(datadir)