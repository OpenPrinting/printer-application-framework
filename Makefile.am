SUBDIRS = server

config_filedir  = $(prefix)/etc/cups/
config_file_DATA = config/framework.config

install-exec-local:
	$(INSTALL_SCRIPT) config/run-server.command $(DESTDIR)/$(bindir)/