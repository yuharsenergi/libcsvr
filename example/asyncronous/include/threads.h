#ifndef THREADS_H
#define THREADS_H

#include <threads.h>
#include "libcsvr.h"

int initThreads(csvrServer_t *input);
void shutdownThreads();
void joinThreads();

#endif