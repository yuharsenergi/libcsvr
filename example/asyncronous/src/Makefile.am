bin_PROGRAMS  = asyncronous

asyncronous_SOURCES = main.c
asyncronous_SOURCES += threads.c
asyncronous_SOURCES += codec.c

asyncronous_LDFLAGS = -lm -lrt -lpthread
asyncronous_LDFLAGS += $(top_srcdir)/lib/libcsvr-1.0.la

asyncronous_CPPFLAGS = -Wall -g
asyncronous_CPPFLAGS += -W
asyncronous_CPPFLAGS += -I$(top_srcdir)/include/ 
asyncronous_CPPFLAGS += -L$(top_srcdir)/lib/
asyncronous_CPPFLAGS += -D_GNU_SOURCE