#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "macros.h"
#include "threads.h"
#include "codec.h"

static pthread_t thrServer;
bool isRunning = false;

int initThreads(csvrServer_t *input);
void shutdownThreads(void);
void joinThreads(void);
void *threadServer(void *arg);

void *threadServer(void *arg)
{
    csvrServer_t *input = (csvrServer_t*)arg;

    isRunning = true;
    csvrRequest_t request;
    while(isRunning == true)
    {
        CLEARSTRUCT(request);
        csvrRead(input, &request);

        char *loggingPurpose = NULL;
        if(request.content)
        {
            if(asprintf(&loggingPurpose, "%s",request.content) != -1)
            {
                trim_lf(loggingPurpose);
            }
        }
        printf("[ <<< ] [%s][%s] %s\n",request.clientAddress, request.path, loggingPurpose ? loggingPurpose : "");
        FREE(loggingPurpose);

        csvrResponse_t response;
        CLEARSTRUCT(response);
        char * jsonData = "{\"status\":\"OK\"}\n";
        csvrAddContent(&response, jsonData);
        csvrSendResponse(input, &request, &response);
        printf("[ >>> ] %s\n",jsonData);
        csvrReadFinish(&request, &response);
    }

    pthread_exit(NULL);
}

int initThreads(csvrServer_t *input)
{
    int ret = -1;
    ret = pthread_create(&thrServer,NULL,threadServer,(void*)input);
    if(ret != 0)
    {
        printf("Failed init server threads!\n");
    }

    return ret;
}

void joinThreads(void)
{
    pthread_join(thrServer,NULL);
}

void shutdownThreads(void)
{
    if(isRunning) pthread_cancel(thrServer);
}