#ifndef LIBCSVR_H
#define LIBCSVR_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CSVR_NAME    "libcsvr"
#define CSVR_VERSION "1.0"

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
    csvrSuccess = 0,
    csvrNotAnError,
    csvrSystemFailure,
    csvrUriAlreadyExists,
    csvrCannotBindingSocket,
    csvrCannotCreateSocket,
    csvrCannotListenSocket,
    csvrCannotAcceptSocket,
    csvrFailedSendData,
    csvrFailedReadSocket,
    csvrInvalidInput,
    csvrInvalidBody,
    csvrInvalidHeader,
    csvrErrMax
}csvrErrCode_e;

typedef enum{
    http1_0 = 0,
    http1_1 = 1
}csvrHttpVersion_e;

typedef enum{
    csvrConnKeepAlive = 0,
    csvrConnClosed    = 1
}csvrConnectionType_e;

typedef enum{
    csvrTypeNotKnown  = 0,
    csvrTypeGet       = 1,
    csvrTypePut       = 2,
    csvrTypePost      = 3,
    csvrTypeHead      = 4,
    csvrTypeDelete    = 5,
    csvrTypeMax
}csvrRequestType_e;

typedef enum{
    noContentType   = 0,
    applicationJson = 1,
    textHtml        = 2,
    maxContentType
}csvrContentType_e;

typedef enum{
    /* Information response */
    csvrResponseContinue            = 100,
    csvrResponseSwitchingProtocol   = 101,
    csvrResponseProcessing          = 102,
    csvrResponseEarlyHints          = 103,

    /* Successful response */
    csvrResponseOk                  = 200,
    csvrResponseCreated             = 201,
    csvrResponseAccepted            = 202,
    csvrResponseNoAuthoritativeInformation = 203,
    csvrResponseNoContent           = 204,
    csvrResponseResetContent        = 205,
    csvrResponsePartialContent      = 206,
    csvrResponseMultiStatus         = 207,
    csvrResponseIMUsed              = 226,

    /* Redirection response */
    
    /* Client Error response */

    /* Server Error response */
    csvrResponseInternalServerError = 500,
    csvrResponseNotImplemented      = 501,
    csvrResponseBadGateway          = 502,
    csvrResponseServiceUnavailable  = 503,
    csvrResponseGatewayTimeout      = 504,
    csvrResponseHttpVersionNotSupported = 505,
    csvrResponseVariantAlsoNegotiates   = 506,
    csvrResponseInsufficientStorage     = 507,
    csvrResponseLoopDetected            = 508,
    csvrResponseNotExtended             = 510,
    csvrResponseNetworkAuthenticationRequired = 511,

}csvrHttpResponseCode_e;

typedef struct{
    size_t total;
    char **data;
}csvrHeader_t;

typedef struct
{
    csvrRequestType_e type;
    csvrContentType_e contentType;
    csvrHttpVersion_e httpVersion;
    csvrConnectionType_e connectionType;
    int clientfd;
    int contentLength;
    char clientAddress[INET_ADDRSTRLEN];
    char host[100];
    char path[100];
    char *message;  /* Full data header + body (if any) */
    char *header;   /* The header data only */
    char *content;  /* The content data only */
}csvrRequest_t;


typedef struct{
    csvrHeader_t header;
    char *body;
}csvrResponse_t;

typedef struct
{
    bool asyncFlag;
    int sockfd;
    char *serverName;
    uint16_t port;
    struct csvrPathUrl_t * path;
}csvrServer_t;

struct csvrPathUrl_t
{
    csvrRequestType_e type;
    char *name;
    void *(*callbackFunction) (csvrServer_t*,csvrRequest_t *, void *);
    struct csvrPathUrl_t *next;
};

csvrErrCode_e csvrInit(csvrServer_t *input, uint16_t port);
csvrErrCode_e csvrServerStart(csvrServer_t *input, void *userData);

csvrErrCode_e csvrSetCustomServerName(csvrServer_t *input, char *serverName,...);
csvrErrCode_e csvrShutdown(csvrServer_t *input);

csvrErrCode_e csvrRead(csvrServer_t *input, csvrRequest_t *output);
csvrErrCode_e csvrReadFinish(csvrRequest_t *input, csvrResponse_t *csvrResponseInput);
csvrErrCode_e csvrSendResponse(csvrServer_t *input, csvrRequest_t * request, csvrResponse_t *response);
csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value);
csvrErrCode_e csvrAddContent(csvrResponse_t*input, char *content,...);
csvrErrCode_e csvrAddPath(csvrServer_t *input, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrServer_t*,csvrRequest_t *, void *));

#endif