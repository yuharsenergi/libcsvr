#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include "libcsvr.h"
#include "macros.h"
#include "threads.h"
#include "codec.h"

int initializeServerPath(csvrServer_t *input);
long session = 0;
void *handlerGetUuid(csvrRequest_t *request, void *userData);
void *handlerTime(csvrRequest_t *request, void *userData);
void *handlerRequest(csvrRequest_t *request, void *userData);

/**
 * @brief Got the example from https://stackoverflow.com/a/51068240/16389548
 * 
 * @return char* 
 */
char *generateUuid()
{
    uuid_t binuuid;
    /*
     * Generate a UUID. We're not done yet, though,
     * for the UUID generated is in binary format 
     * (hence the variable name). We must 'unparse' 
     * binuuid to get a usable 36-character string.
     */
    uuid_generate_random(binuuid);

    /*
     * uuid_unparse() doesn't allocate memory for itself, so do that with
     * malloc(). 37 is the length of a UUID (36 characters), plus '\0'.
     */
    char *uuid = malloc(37);

#ifdef capitaluuid
    /* Produces a UUID string at uuid consisting of capital letters. */
    uuid_unparse_upper(binuuid, uuid);
#elif lowercaseuuid
    /* Produces a UUID string at uuid consisting of lower-case letters. */
    uuid_unparse_lower(binuuid, uuid);
#else
    /*
     * Produces a UUID string at uuid consisting of letters
     * whose case depends on the system's locale.
     */
    uuid_unparse(binuuid, uuid);
#endif
    uuid[strlen(uuid)] = 0;
    return uuid;
}

char* printTime()
{
    time_t debug_time;
    time(&debug_time);

    struct timeval tv;
    gettimeofday(&tv, NULL); // timezone should be NULL
    struct tm *d_tm;
    d_tm = localtime(&tv.tv_sec);
    __suseconds_t msec = tv.tv_usec/1000;
    static char time_str[30];
    memset(time_str, 0x00, 24*sizeof(char));
    strftime(time_str, sizeof(time_str), "%y-%m-%d %H:%M:%S", d_tm);
    snprintf(time_str + strlen(time_str),sizeof(time_str), ".%03lu",msec);
    return time_str;
}

void *handlerGetUuid(csvrRequest_t *request, void *userData)
{
    printf("[ <<< ] [%s][%s] %s\n",request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);
    char *uuid = generateUuid();
    long total = csvrGetTotalConnection();
    csvrAddContent(&response, applicationJson,"{\"id\":%d,\"totalConnection\":%ld,\"uuid\":\"%s\"}",session++, total, uuid);
    FREE(uuid);
    
    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);
    FREE(request);

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
    snprintf(time_str + strlen(time_str),sizeof(time_str), ".%03lu",msec);
    usleep(300000);

    csvrAddCustomHeader(&response,"header1","test/custom-header");
    csvrAddCustomHeader(&response,"header2","random");

    csvrAddContent(&response,applicationJson,"{\"id\":%d,\"time\":\"%s\"}",session++,time_str);
    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);
    FREE(request);

    return NULL;
}

void *handlerRequest(csvrRequest_t *request, void *userData)
{
    printf("[ <<< ] [%s][%s][%s] %s\n",printTime(), request->clientAddress, request->path,request->content ? request->content : "");
    csvrResponse_t response;
    CLEARSTRUCT(response);

    if(request->type == csvrTypePost)
    {
        csvrAddContent(&response, applicationJson,"{\"id\":%d,\"request\":%s}", session,request->content ? request->content : "\"\"");
    }
    else
    {
        csvrAddContent(&response, applicationJson,"{\"status\":\"ok\"}");
    }

    csvrSendResponse(request, &response);
    printf("[ >>> ] %s\n",response.body);
    csvrReadFinish(request, &response);
    FREE(request);

    return NULL;
}

int initializeServerPath(csvrServer_t *server)
{
    csvrAddPath(server, "/", csvrTypePost, handlerRequest, NULL);
    csvrAddPath(server, "/", csvrTypeGet, handlerRequest, NULL);
    csvrAddPath(server, "/time", csvrTypeGet, handlerTime, NULL);
    csvrAddPath(server, "/uuid", csvrTypeGet, handlerGetUuid, NULL);
    printf("Initialize URI Path finished.\n");
    return 0;
}