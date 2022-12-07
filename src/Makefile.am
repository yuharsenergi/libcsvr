lib_LTLIBRARIES = libcsvr-@PACKAGE_VERSION@.la

libcsvr_@PACKAGE_VERSION@_la_SOURCES = libcsvr.c
libcsvr_@PACKAGE_VERSION@_la_SOURCES += libcsvr_response.c
libcsvr_@PACKAGE_VERSION@_la_SOURCES += libcsvr_signal.c

libcsvr_@PACKAGE_VERSION@_la_LDFLAGS = -lpthread

libcsvr_@PACKAGE_VERSION@_la_CPPFLAGS  = -Wall -g
libcsvr_@PACKAGE_VERSION@_la_CPPFLAGS += -I$(top_srcdir)/include
libcsvr_@PACKAGE_VERSION@_la_CPPFLAGS += -D_GNU_SOURCE
