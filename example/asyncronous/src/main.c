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

static csvrServer_t * server = NULL;

int main(int arg,char**argv)
{
    printf("%s-%s\n",EXAMPLE_NAME,EXAMPLE_VERSION);
    if(arg < 2)
    {
        printf("\n-- Please specify port --\n\n");
        printf("Usage :\n");
        printf("    %s [port]\n", argv[0]);
        printf("Example :\n");
        printf("    %s 9000\n\n", argv[0]);
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

    csvrSetMaxRequestAllowed(1000000);
    server = csvrInit((uint16_t)port);
    if(server == NULL)
    {
        printf("Failed initialize server at port:%u\n",port);
        return 0;
    }
    printf("Success init server\n");

    if(initializeServerPath(server) != 0)
    {
        csvrShutdown(server);
        return 0;
    }

    if(csvrServerStart(server) == csvrSuccess)
    {
        printf("Server is running\n");
    }
    else
    {
        csvrShutdown(server);
        printf("Cannot start server\n");
        return -1;
    }

    csvrJoin(server);

    csvrShutdown(server);

    printf("\n\n-- Server is dead. Thank you.\n\n");

    return 0;
}