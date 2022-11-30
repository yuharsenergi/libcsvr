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

typedef struct
{
    csvrRequest_t *request;
    csvrResponse_t *response;
}cbHandler_t;

void threadsCallback(void *arg)
{
    cbHandler_t* data = (cbHandler_t*)arg;
    if(data)
    {
        csvrReadFinish(data->request, data->response);
        FREE(data->request);
        FREE(data->response);
        FREE(data);
    }
    printf("Server threads is dead!\n");
    isRunning = false;
}

void *threadServer(void *arg)
{
    csvrServer_t *server = (csvrServer_t*)arg;

    isRunning = true;
    cbHandler_t *cbHandler = NULL;
    csvrRequest_t *request = NULL;
    csvrResponse_t *response = NULL;
    cbHandler = calloc(1, sizeof(cbHandler_t));
    if(cbHandler == NULL)
    {
        isRunning = false;
        pthread_exit(NULL);
    }

    pthread_cleanup_push(threadsCallback, (void*)cbHandler);
    if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
    {
        FREE(cbHandler);
        pthread_exit(NULL);
    }

    while(1)
    {
        request = calloc(1, sizeof(csvrRequest_t));
        response = calloc(1, sizeof(csvrResponse_t));
        if(request == NULL || response == NULL)
        {
            printf("Failed allocating request/response memory");
            break;
        }

        memset(request, 0, sizeof(csvrRequest_t));
        memset(response, 0, sizeof(csvrResponse_t));

        /* Save pointer to cb handler, the pointer will be freed during pthread cancelation */
        cbHandler->request  = request;
        cbHandler->response = response;

        csvrRead(server, request);
        printf("[ <<< ] [%s][%s] %s\n",request->clientAddress, request->path, request->content ? request->content : "");

        csvrAddContent(response, "{\"status\":\"OK\"}");
        csvrSendResponse(request, response);
        printf("[ >>> ] %s\n",response->body);
        csvrReadFinish(request, response);
        free(request);
        free(response);
        request = NULL;
        response = NULL;
    }
    FREE(cbHandler);
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

int initThreads(csvrServer_t *server)
{
    int ret = -1;
    ret = pthread_create(&thrServer,NULL,threadServer,(void*)server);
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
    pthread_join(thrServer,NULL);
}