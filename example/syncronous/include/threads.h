#ifndef THREADS_H
#define THREADS_H

#include <threads.h>
#include "libcsvr.h"

int initThreads(csvrServer_t *);
void shutdownThreads(void);
void joinThreads(void);

#endif