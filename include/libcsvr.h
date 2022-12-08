/********************************************************************************
 * @file libcsvr.h
 * @author Ergi (yuharsenergi@gmail.com)
 * @brief Collection functions of all the csvr main runtime functions
 * @version 0.1
 * @date 2022-11-27
 * 
 * @copyright Copyright (c) 2022 Yuharsen Ergi
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
********************************************************************************/
#ifndef LIBCSVR_H
#define LIBCSVR_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>

/** The csvr signature name *///
#define CSVR_NAME    "libcsvr"
/** The current csvr version *///
#define CSVR_VERSION "1.0"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef __linux__
#include <arpa/inet.h>
#endif

/***************************************************************************************************************
 * @brief Enumeration to describe any return value from almost all the libcsvr functions.
 ***************************************************************************************************************/
typedef enum
{
    csvrSuccess             = 0,    /**< If any procedure is complete successfully. */
    csvrNotAnError          = 1,    /**< If the procedure shows a process that is not a success but is not also an error. */
    csvrSystemFailure       = 2,    /**< If any procedure shows a failure process. For example : Failed during allocating memory for dynamic allocation variable. */
    csvrUriAlreadyExists    = 3,    /**< If any URI is already exists in the csvrPathUrl_t linked list object. */
    csvrCannotBindingSocket = 4,    /**< If cannot bind requested port during csvrInit binding procedure. Probably the port already in used by another process. See the bind() programming manual. */
    csvrCannotCreateSocket  = 5,    /**< If cannot create TCP socket. Currently this enumeration is not used in any functions return value. */
    csvrCannotListenSocket  = 6,    /**< If cannot listen to the opened socket by csvrInit. Any error is describe in the errno variable from the listen() procedure. See the listen() programming manual. */
    csvrCannotAcceptSocket  = 7,    /**< If cannot accept incoming request to the opened socket by csvrInit. Any error is describe in the errno variable from the accept() procedure. See the accept() programming manual. */
    csvrFailedSendData      = 8,    /**< If cannot send data during sending response to the client socket which has been created by accept() functions. See the send() programming manual. */
    csvrFailedReadSocket    = 9,    /**< If cannot read data during sending response to the client socket which has been created by accept() functions. See the read() programming manual. */
    csvrInvalidInput        = 10,   /**< If any of the input function arguments are invalid. For example a NULL value assigned to a function argument that does not accept NULL pointer. */
    csvrInvalidBody         = 11,   /**< If the body pointer in the csvrResponse_t structure is NULL. The user must set the body or content of the response by calling csvrAddContent function before calling csvrSendResponse. */
    csvrInvalidHeader       = 12,   /**< If cannot get the path URI of the incoming request header data in the getRequestUriPath function. */
    csvrNoContentLength     = 13,   /**< If no content length found during checking the incoming header data from a POST type request in the csvrClientReader function. */
    csvrErrMax
}csvrErrCode_e;

/***************************************************************************************************************
 * @brief Enumeration to describe which HTTP format version is being used.
 ***************************************************************************************************************/
typedef enum{
    http1_0 = 0,
    http1_1 = 1
}csvrHttpVersion_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the Connection-Type in the process.
 ***************************************************************************************************************/
typedef enum{
    csvrConnKeepAlive = 0,
    csvrConnClosed    = 1
}csvrConnectionType_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the incoming HTTP request type.
 *        Current libcsvr version only support PUT type. 
 *        Any other request will be handled as GET type request.
 ***************************************************************************************************************/
typedef enum{
    csvrTypeNotKnown  = 0,
    csvrTypeGet       = 1,
    csvrTypePut       = 2,
    csvrTypePost      = 3,
    csvrTypeHead      = 4,
    csvrTypeDelete    = 5,
    csvrTypeUnknwon   = 6,
    csvrTypeMax
}csvrRequestType_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the Content-Type of the incoming POST data body.
 ***************************************************************************************************************/
typedef enum{
    noContentType   = 0,
    applicationJson = 1,
    textHtml        = 2,
    maxContentType
}csvrContentType_e;

/***************************************************************************************************************
 * @brief Enumeration to set the HTTP response code.
 ***************************************************************************************************************/
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
    csvrResponseBadRequest          = 400,
    csvrResponseUnauthorized        = 401,
    csvrResponsePaymentRequired     = 402,
    csvrResponseForbidden           = 403,
    csvrResponseNotFound            = 404,
    csvrResponseMethodNotAllowed    = 405,
    csvrResponseNotAcceptable       = 406,
    csvrResponseRequestTimeout      = 408,
    csvrResponseLengthRequired      = 411,

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

/***************************************************************************************************************
 * @brief Structure save all the user custom header information which is set by calling csvrAddCustomHeader function.
 ***************************************************************************************************************/
typedef struct{
    size_t total;
    char **data;
}csvrHeader_t;

/***************************************************************************************************************
 * @brief Structure to save all the client information (like socket file descriptor, and the client address,
 *        payload, header, content data, etc) during reading the client socket.
 ***************************************************************************************************************/
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
    char *serverName;   /* The copied server name */
    char *message;      /* Full data header + body (if any) */
    char *header;       /* The header data only */
    char *content;      /* The content data only */
}csvrRequest_t;

/***************************************************************************************************************
 * @brief Structure to save all user response information. The user must call csvrAddCustomHeader to set custom
 *        response HTTP header, and call csvrAddContent to set the response content body/payload.
 ***************************************************************************************************************/
typedef struct{
    csvrHeader_t header;
    char *body;
}csvrResponse_t;

/***************************************************************************************************************
 * @brief Main structure to save all the csvr configuration, socket descriptor, and user path linked list.
 *        This structure will continue to be used as long as the user server is running.
 ***************************************************************************************************************/
typedef struct
{
    bool asyncFlag; /* To tell libcsvr that the server is running in async mode. */

    int sockfd;
    char *serverName;
    uint16_t port;
    struct csvrPathUrl_t * path;
}csvrServer_t;

/***************************************************************************************************************
 * @brief Structure to save all the user requested URI path by calling the csvrAddPath function.
 ***************************************************************************************************************/
struct csvrPathUrl_t
{
    csvrRequestType_e type;
    char *name;
    void *(*callbackFunction) (csvrRequest_t *, void *);
    struct csvrPathUrl_t *next;
};

csvrServer_t *csvrInit(uint16_t port);
csvrErrCode_e csvrServerStart(csvrServer_t *server, void *userData);
csvrErrCode_e csvrJoin(csvrServer_t *server);

csvrErrCode_e csvrSetCustomServerName(csvrServer_t *input, char *serverName,...);
csvrErrCode_e csvrShutdown(csvrServer_t *input);

csvrErrCode_e csvrRead(csvrServer_t *input, csvrRequest_t *output);
csvrErrCode_e csvrReadFinish(csvrRequest_t *request, csvrResponse_t *csvrResponseInput);
csvrErrCode_e csvrSendResponse(csvrRequest_t * request, csvrResponse_t *response);
csvrErrCode_e csvrSendResponseError(csvrRequest_t * request, csvrHttpResponseCode_e code, char*desc);
csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value);
csvrErrCode_e csvrAddContent(csvrResponse_t*input, char *content,...);
csvrErrCode_e csvrAddPath(csvrServer_t *input, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrRequest_t *, void *));
long csvrGetTotalConnection();

#endif