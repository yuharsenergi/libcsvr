#ifndef LIBSERVER_H
#define LIBSERVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**< Library untuk socket programming *///
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef __linux__
#include <arpa/inet.h>
#endif

typedef enum
{
    errSuccess = 0,
    errSystemFailure,
    errCannotCreateSocket,
    errBindingFailed,
    errCannotListenSocket,
    errCannotAcceptSocket,
    errCannotSendData,
    errInvalidInput,
    errInvalidBody,
    errReadFailed,
    errMaxlen
}serverErrorCode_e;

typedef enum{
    http1_0 = 0,
    http1_1 = 1
}httpVersion_e;

typedef enum{
    connKeepAlive = 0,
    connClose     = 1
}connectionType_e;

typedef enum{
    notKnown  = 0,
    get       = 1,
    put       = 2,
    post      = 3,
    head      = 4,
    delete    = 5,
    maxType
}requestType_e;

typedef enum{
    applicationJson = 0,
    textHtml        = 1,
    maxContentType
}contentType_e;

typedef enum{
    /* Information Response */
    responseContinue            = 100,
    responseSwitchingProtocol   = 101,
    responseProcessing          = 102,
    responseEarlyHints          = 103,

    /* Successful response */
    responseOk                  = 200,
    responseCreated             = 201,
    responseAccepted            = 202,
    responseNoAuthoritativeInformation = 203,
    responseNoContent           = 204,
    responseResetContent        = 205,
    responsePartialContent      = 206,
    responseMultiStatus         = 207,
    responseIMUsed              = 226,

    /* Redirection response */
    
    /* Client Error response */

    /* Server Error response */
    responseInternalServerError = 500,
    responseNotImplemented      = 501,
    responseBadGateway          = 502,
    responseServiceUnavailable  = 503,
    responseGatewayTimeout      = 504,
    responseHttpVersionNotSupported = 505,
    responseVariantAlsoNegotiates   = 506,
    responseInsufficientStorage     = 507,
    responseLoopDetected            = 508,
    responseNotExtended             = 510,
    responseNetworkAuthenticationRequired = 511,

}httpResponseCode_e;

typedef struct{
    size_t total;
    char **data;
}header_t;

typedef struct
{
    requestType_e type;
    contentType_e contentType;
    httpVersion_e httpVersion;
    connectionType_e connectionType;
    size_t contentLength;
    char clientAddress[INET_ADDRSTRLEN];
    char host[100];
    char path[100];
    char *message;  /* Full data header + body (if any) */
    char *header;   /* The header data only */
    char *content;  /* The content data only */
}request_t;


typedef struct{
    header_t header;
    char *body;
}response_t;

typedef struct
{
    int sockfd;
    int clientfd;
    uint16_t port;
}server_t;

serverErrorCode_e serverInit(server_t *input, uint16_t port);
serverErrorCode_e serverShutdown(server_t *input);

serverErrorCode_e serverRead(server_t *input, request_t *output);
serverErrorCode_e serverReadFinish(request_t *input, response_t *responseInput);
serverErrorCode_e serverSend(server_t *input, response_t *responseInput);
serverErrorCode_e serverAddCustomHeader(response_t*input, char *key, char*value);
serverErrorCode_e serverAddContent(response_t*input, char *content,...);

size_t getContentLength(char*header);

#endif