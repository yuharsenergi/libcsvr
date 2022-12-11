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

#include "libcsvr.h"
#include "libcsvr_tls.h"
#include "threads.h"

#define EXAMPLE_NAME "syncronousTlsExample"
#define EXAMPLE_VERSION "1.0"

static csvrTlsServer_t * server = NULL;
static sem_t semKill;

static void signalCallback()
{
    sem_post(&semKill);
}

int main(int arg,char**argv)
{
    printf("%s-%s\n",EXAMPLE_NAME,EXAMPLE_VERSION);
    if(arg < 4)
    {
        printf("\n-- Please specify port --\n\n");
        printf("Usage :\n");
        printf("    %s [port]\n", argv[0]);
        printf("Example :\n");
        printf("    %s 8088\n\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if(port < 1024)
    {
        if(port == 0)
        {
            printf("\n-- port %s not valid ! --\n\n", argv[1]);
        }
        else
        {
            printf("\n-- Port %6s not valid ! --\n", argv[1]);
            printf("-- Please use port > 1024. --\n\n");
        }
        return 1;
    }

    server = csvrTLSInit((uint16_t)port, argv[2], argv[3]);
    if(server == NULL)
    {
        printf("Failed initialize server at port:%u\n",port);
        return 0;
    }
    printf("Success init server\n");

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

    do
    {
        if(initThreads(server) != 0)
        {
            break;
        }
        server->server->path = NULL;
        if(!sem_wait(&semKill))
        {
            break;
        }
    }while(0);

    csvrTlsShutdown(server);

    sem_destroy(&semKill);

    printf("\n\n-- Server is dead. Thank you.\n\n");

    return 0;
}