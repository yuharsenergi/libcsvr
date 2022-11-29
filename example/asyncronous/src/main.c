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

#include "threads.h"
#include "macros.h"
#include "libcsvr.h"

#define EXAMPLE_NAME "asyscronousExample"
#define EXAMPLE_VERSION "1.0"

#define PORT 9000

static sem_t semKill;

static void signalCallback()
{
    sem_post(&semKill);
}

int main(int arg,char**argv)
{
    printf("%s-%s\n",EXAMPLE_NAME,EXAMPLE_VERSION);

    struct sigaction sigintHandler;
    struct sigaction sigtermHandler;

    /* initialize semaphore */
    sem_init(&semKill, 0, 0);

    /* Signal handler for SIGINT */
    memset(&sigintHandler, 0, sizeof(sigintHandler));
    sigintHandler.sa_sigaction = &signalCallback;
    sigintHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGINT, &sigintHandler, NULL);

    /* Signal handler for SIGTERM */
    memset(&sigtermHandler, 0, sizeof(sigtermHandler));
    sigtermHandler.sa_sigaction = &signalCallback;
    sigtermHandler.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &sigtermHandler, NULL);

    csvrServer_t server;
    // server = calloc(1, sizeof(csvrServer_t));
    memset(&server, 0, sizeof(csvrServer_t));

    if(csvrInit(&server, PORT) != csvrSuccess)
    {
        printf("Failed initialize server at port:%u\n",PORT);
        return 0;
    }
    printf("Success init server\n");

    if(initializeServerPath(&server) != 0)
    {
        csvrShutdown(&server);
        return 0;
    }

    if(csvrServerStart(&server, NULL) == csvrSuccess)
    {
        printf("Server is running\n");
    }
    else
    {
        csvrShutdown(&server);
        printf("Cannot start server\n");
        return -1;
    }

    while(1)
    {
        if (!sem_wait(&semKill))
        {
            break;
        }
    }

    csvrShutdown(&server);

    printf("\n\n-- Server is dead. Thank you.\n\n");

    return 0;
}