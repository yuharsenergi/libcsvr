bin_PROGRAMS  = syncronous_tls

syncronous_tls_SOURCES = main.c
syncronous_tls_SOURCES += threads.c

syncronous_tls_LDFLAGS = -lm -lrt -lpthread -luuid -lssl -lcrypto
syncronous_tls_LDFLAGS += $(top_srcdir)/../lib/libcsvr-1.0.la

syncronous_tls_CPPFLAGS = -Wall -g
syncronous_tls_CPPFLAGS += -W
syncronous_tls_CPPFLAGS += -I$(top_srcdir)/include/
syncronous_tls_CPPFLAGS += -I$(top_srcdir)/../include/ 
syncronous_tls_CPPFLAGS += -L$(top_srcdir)/../lib/
syncronous_tls_CPPFLAGS += -D_GNU_SOURCE