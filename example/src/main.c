#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <syslog.h>

#include "config.h"
#include "threads.h"
#include "macros.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "example"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0"
#endif

#define PORT 9000

static sem_t semKill;

static void termSignalHandler()
{
    sem_post(&semKill);
}

int main(int arg,char**argv)
{
    printf("%s-%s\n",PACKAGE_NAME,PACKAGE_VERSION);

    ssize_t retVal = 0;

    struct sigaction SIGINTHandler;
    struct sigaction SIGTERMHandler;

    /* initialize semaphore */
    sem_init(&semKill, 0, 0);

    /* assign signal handler for SIGINT */
    memset(&SIGINTHandler, 0, sizeof(SIGINTHandler));
    SIGINTHandler.sa_sigaction = &termSignalHandler;
    SIGINTHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &SIGINTHandler, NULL);
    /* assign signal handler for SIGINT -- end */

    /* assign signal handler for SIGTERM */
    memset(&SIGTERMHandler, 0, sizeof(SIGTERMHandler));
    SIGTERMHandler.sa_sigaction = &termSignalHandler;
    SIGTERMHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &SIGTERMHandler, NULL);
    /* assign signal handler for SIGTERM -- end */

    csvrServer_t server;
    memset(&server, 0, sizeof(csvrServer_t));

    if(csvrInit(&server, PORT) != csvrSuccess)
    {
        printf("Failed initialize server at port:%u\n",PORT);
        return -1;
    }
    printf("Success init server\n");

    if(initThreads(&server) != 0)
    {
        csvrShutdown(&server);
        printf("Failed create threads\n");
        return -1;
    }
    printf("Server is running\n");

    while(1)
    {
        if (!sem_wait(&semKill))
        {
            break;
        }
    }

    shutdownThreads();
    csvrShutdown(&server);
    joinThreads();

    return 0;
}