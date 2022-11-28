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

int initializeServerPath(csvrServer_t *input);
long session = 0;
void *handlerGetData(csvrRequest_t *request, void *userData);
void *handlerTime(csvrRequest_t *request, void *userData);
void *handlerRequest(csvrRequest_t *request, void *userData);

void *handlerGetData(csvrRequest_t *request, void *userData)
{
    printf("[ <<< ] [%s][%s] %s\n",request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);

    csvrAddContent(&response, "{\"id\":%d,\"status\":\"Get roamer Data\"}\n",session);
    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);

    return NULL;
}

void *handlerTime(csvrRequest_t *request, void *userData)
{
    printf("[ <<< ] [%s][%s]\n", request->clientAddress, request->path);
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

    csvrAddContent(&response,
            "{"
                "\"id\":%d,"
                "\"time\":\"%s\""
            "}",
            session++,
            time_str);
    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);
    return NULL;
}

void *handlerRequest(csvrRequest_t *request, void *userData)
{
    printf("[ <<< ] [%s][%s] %s\n",request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);
    csvrAddContent(&response, "{\"id\":%d,\"request\":%s}", session,request->content ? request->content : "\"\"");

    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);

    return NULL;
}

int initializeServerPath(csvrServer_t *input)
{
    csvrAddPath(input, "/", csvrTypePost, handlerRequest);
    csvrAddPath(input, "/time", csvrTypeGet, handlerTime);
    csvrAddPath(input, "/roamer", csvrTypePost, handlerGetData);
    printf("Initialize URI Path finished.\n");
    return 0;
}