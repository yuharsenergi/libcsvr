bin_PROGRAMS  = syncronous

syncronous_SOURCES = main.c
syncronous_SOURCES += threads.c
syncronous_SOURCES += codec.c

syncronous_LDFLAGS = -lm -lrt -lpthread
syncronous_LDFLAGS += $(top_srcdir)/../../lib/lib/libcsvr.la

syncronous_CPPFLAGS = -Wall -g
syncronous_CPPFLAGS += -W
syncronous_CPPFLAGS += -I$(top_srcdir)/include/ 
syncronous_CPPFLAGS += -L$(top_srcdir)/../../lib/lib/
syncronous_CPPFLAGS += -I$(top_srcdir)/../../lib/include/
syncronous_CPPFLAGS += -D_GNU_SOURCE