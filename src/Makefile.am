lib_LTLIBRARIES = libcsvr.la

libcsvr_la_SOURCES = libcsvr.c
libcsvr_la_SOURCES += libcsvr_response.c
libcsvr_la_SOURCES += libcsvr_signal.c
libcsvr_la_SOURCES += libcsvr_tls.c

libcsvr_la_LDFLAGS = -lpthread -version-info @PACKAGE_VERSION@
libcsvr_la_LDFLAGS += -lssl -lcrypto

libcsvr_la_CPPFLAGS  = -Wall -g
libcsvr_la_CPPFLAGS += -I$(top_srcdir)/include
libcsvr_la_CPPFLAGS += -D_GNU_SOURCE
