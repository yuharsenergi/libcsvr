#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "libcsvr.h"
#include "libcsvr_tls.h"
#include "threads.h"

static pthread_t thrServer;
bool isRunning = false;
bool exitSignal = false;

void shutdownThreads(void);
void joinThreads(void);
void *threadServer(void *);

typedef struct
{
    csvrTlsRequest_t *request;
}cbHandler_t;

void threadsCallback(void *arg)
{
    cbHandler_t* data = (cbHandler_t*)arg;
    if(data)
    {
        if(data->request){
            free(data->request);
        }
        free(data);
    }
    printf("Server threads is dead!\n");
    isRunning = false;
}

void *threadServer(void *arg)
{
    csvrTlsServer_t *server = (csvrTlsServer_t*)arg;
    cbHandler_t* data = NULL;
    data = calloc(1, sizeof(cbHandler_t));
    if(data == NULL)
    {
        pthread_exit(NULL);
    }
    pthread_cleanup_push(threadsCallback, (void*)data);
    if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
    {
        pthread_exit(NULL);
    }

    printf("[DEBUG] Server is listening...\n");
    isRunning = true;

    int retprint = 0;
    while(1)
    {
        csvrTlsRequest_t * request = NULL;
        request = calloc(1,sizeof(csvrTlsRequest_t));
        if(request == NULL) break;

        memset(request,0, sizeof(csvrTlsRequest_t));
        if(csvrTlsRead(server, request) == csvrSuccess)
        {
            printf("[ <<< %s:%u] %s\n",request->address, request->port, request->data.message ? request->data.message : "");
            char *response = NULL;
            if(request->data.type == csvrTypePost)
            {
                retprint = asprintf(&response, "{"
                    "\"status\":\"OK\","
                    "\"type\":%d,"
                    "\"message\":\"%s\""
                "}", 
                request->data.type,
                request->data.message
                );
                
                if(retprint == -1)
                {
                    csvrTlsReadFinish(request);
                    free(request);
                    continue;
                }
            }
            else
            {
                retprint = asprintf(&response, "{"
                    "\"status\":\"OK\","
                    "\"type\":%d,"
                    "\"message\":\"Hello!\""
                "}", 
                request->data.type);

                if(retprint == -1)
                {
                    csvrTlsReadFinish(request);
                    free(request);
                    continue;
                }
            }
            csvrTlsSend(server, request, response, strlen(response));
            printf("[ >>> ] %s\n",response);
            free(response);
        }
        else
        {
            if(exitSignal) break;
            printf("Cannot read socket TLS\n");
        }
        csvrTlsReadFinish(request);
        free(request);
    }
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

int initThreads(csvrTlsServer_t *server)
{
    int ret = -1;
    if(!csvrCheckRoot())
    {
        printf("Application must be run as 'root'\n");
        return -1;
    }
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
    exitSignal = true;
    if(isRunning)
    {
        pthread_cancel(thrServer);
        pthread_join(thrServer,NULL);
    }
}