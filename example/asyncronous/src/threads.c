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
void *handlerGetData(csvrServer_t *server, csvrRequest_t *request, void *userData);
void *handlerTime(csvrServer_t *server, csvrRequest_t *request, void *userData);
void *handlerRequest(csvrServer_t *server, csvrRequest_t *request, void *userData);

void *handlerGetData(csvrServer_t *server,csvrRequest_t *request, void *userData)
{
    printf("[%s][ <<< ] [%s][%s] %s\n",__func__,request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);
    char * jsonData = "{\"status\":\"Get roamer Data\"}\n";
    csvrAddContent(&response, jsonData);
    csvrSendResponse(server, request, &response);
    printf("[ >>> ] %s\n",jsonData);
    csvrReadFinish(request, &response);

    return NULL;
}

void *handlerTime(csvrServer_t *server,csvrRequest_t *request, void *userData)
{
    printf("[%s][ <<< ] [%s][%s]\n",__func__, request->clientAddress, request->path);
    csvrResponse_t response;
    CLEARSTRUCT(response);

    time_t debug_time;
    time(&debug_time);

    struct timeval tv;
    gettimeofday(&tv, NULL); // timezone should be NULL
    struct tm *d_tm;
    d_tm = localtime(&tv.tv_sec);
    __suseconds_t msec = tv.tv_usec/1000;
    char time_str[30];
    memset(time_str, 0x00, 24*sizeof(char));
    strftime(time_str, sizeof(time_str), "%y-%m-%d %H:%M:%S", d_tm);
    snprintf(time_str + strlen(time_str),sizeof(time_str), ".%lu",msec);

    char * jsonData = NULL;
    if(asprintf(&jsonData, "{\"time\":\"%s\"}\n", time_str) != -1)
    {
        csvrAddContent(&response, jsonData);
    }
    {
        csvrAddContent(&response, "{\"time\":\"\"}\n");
    }
    csvrSendResponse(server, request, &response);
    printf("[ >>> ] %s\n",jsonData);
    csvrReadFinish(request, &response);
    FREE(jsonData);
    return NULL;
}

void *handlerRequest(csvrServer_t *server,csvrRequest_t *request, void *userData)
{
    printf("[%s][ <<< ] [%s][%s] %s\n",__func__,request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);

    char *defaultResponse = "{\"request\":\"ok\"}\n";
    char * jsonData = NULL;
    int retprint = 0;
    if(request->content)
    {
        retprint = asprintf(&jsonData, "{\"request\":\"%s\"}\n", request->content);
    }

    if(retprint != -1)
    {
        csvrAddContent(&response, jsonData);
        printf("[ >>> ] %s\n",jsonData);
    }
    else
    {
        csvrAddContent(&response, defaultResponse);
        printf("[ >>> ] %s\n",jsonData);
    }
    csvrSendResponse(server, request, &response);
    csvrReadFinish(request, &response);
    FREE(jsonData);

    return NULL;
}

void *threadServer(void *arg)
{
    csvrServer_t *input = (csvrServer_t*)arg;

    isRunning = true;
    csvrRequest_t request;
    while(isRunning == true)
    {
        CLEARSTRUCT(request);
        csvrRead(input, &request);

        if(request.content) trim_lf(request.content);

        printf("[ <<< ] [%s][%s] %u %s\n",request.clientAddress, request.path, request.contentLength,request.content ? request.content : "");

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
    csvrAddPath(input, "/", csvrTypePost, handlerRequest);
    csvrAddPath(input, "/time", csvrTypeGet, handlerTime);
    csvrAddPath(input, "/roamer", csvrTypePost, handlerGetData);
    if(csvrServerStart(input, NULL) == csvrSuccess)
    {
        ret = 0;
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