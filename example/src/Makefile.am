bin_PROGRAMS  = example

example_SOURCES = main.c
example_SOURCES += threads.c
example_SOURCES += codec.c

example_LDFLAGS = -lm -lrt
example_LDFLAGS += $(top_srcdir)/../src/.libs/libcsvr.la

example_CPPFLAGS = -Wall -g
example_CPPFLAGS += -W
example_CPPFLAGS += -I$(top_srcdir)/include/ 
example_CPPFLAGS += -L$(top_srcdir)/../src/.libs/
example_CPPFLAGS += -I$(top_srcdir)/../include/
example_CPPFLAGS += -D_GNU_SOURCE