SUBDIRS=src
ACLOCAL_AMFLAGS = -I m4

install-data-local:
# Config file
	@if [ ! -d "$(prefix)/include" ]; then mkdir -p $(prefix)/include; fi
	cp -vr $(top_srcdir)/include/*.h $(prefix)/include/