#ifndef THREADS_H
#define THREADS_H

#include "libcsvr.h"
#include "libcsvr_tls.h"

int initThreads(csvrTlsServer_t *);
void shutdownThreads(void);

#endif