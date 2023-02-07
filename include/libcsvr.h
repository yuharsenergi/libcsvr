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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>

/** The csvr signature name *///
#define CSVR_NAME    "libcsvr"
/** The current csvr version *///
#define CSVR_VERSION "1.0.1"
/** Macro helper to free pointer memory. */
#define CSVR_FREE(ptr) if(ptr != NULL) {free(ptr);ptr = NULL;}
/** Macro helper to print the errno information. */
#define CSVR_PRINT_ERRNO() {printf("errno : (%d) %s\n", errno,strerror(errno));}

#define CSVR_HEADER_SEPARATOR          "\x0D\x0A"
#define CSVR_BODY_SEPARATOR            "\x0D\x0A\x0D\x0A"

/** Macro helper to determine the environment information. */
#ifdef CSVR_UNIT_TEST
/** This macro will be used for libcsvr unit testing purpose. */
#define CSVR_STATIC
#else
#define CSVR_STATIC static
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef __linux__
#include <arpa/inet.h>
#endif

#ifdef MSDOS
#include <dos.h>
#endif

/* Macro helper for SLEEP */
#if defined(MSDOS)
    #define USLEEP(ms) {delay(ms);}
#elif defined(WIN32)
    #define USLEEP(ms) {Sleep(ms);}
#else
    #define USLEEP(ms) {usleep(ms);}
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
    http1_0 = 0,        /**< If the HTTP version is HTTP/1.0 */
    http1_1 = 1         /**< If the HTTP version is HTTP/1.1 */
}csvrHttpVersion_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the Connection-Type in the process.
 ***************************************************************************************************************/
typedef enum{
    csvrConnKeepAlive = 0,  /**< If incoming request is Connection keep-alive */
    csvrConnClosed    = 1   /**< If incoming request is Connection closed */
}csvrConnectionType_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the incoming HTTP request type.
 *        Current libcsvr version only support PUT type. 
 *        Any other request will be handled as GET type request.
 ***************************************************************************************************************/
typedef enum{
    csvrTypeNotKnown  = 0,      /**< If the csvrRequestType_e variable is not set. Maybe the csvrRequest_t variable was just newly created. */
    csvrTypeGet       = 1,      /**< If incoming request type is GET */
    csvrTypePut       = 2,      /**< If incoming request type is PUT */
    csvrTypePost      = 3,      /**< If incoming request type is POST */
    csvrTypeHead      = 4,      /**< If incoming request type is HEAD */
    csvrTypeDelete    = 5,      /**< If incoming request type is DELETE */
    csvrTypeUnknwon   = 6,      /**< If incoming request type is not known  */
    csvrTypeMax
}csvrRequestType_e;

/***************************************************************************************************************
 * @brief Enumeration to describe the Content-Type of the incoming POST data body.
 ***************************************************************************************************************/
typedef enum{
    noContentType   = 0,        /**< If no content-type key not found in the incoming header request payload. Usually it comes from a not POST request. */
    applicationJson = 1,        /**< If the content-type of the incoming body request payload is "application/json". Usually it comes from a POST request, or to set application/json content-type in the response payload. */
    applicationJs   = 2,        /**< If the content-type of the incoming body request payload is "application/javascript". Usually it comes from a POST request, or to set application/javascript content-type in the response payload. */
    textHtml        = 3,        /**< If the content-type of the incoming body request payload is "text/html". Usually it comes from a POST request, or to set text/html content-type in the response payload */
    textPlain       = 4,        /**< If the content-type of the incoming body request payload is "text/plain". Usually it comes from a POST request, or to set text/plain content-type in the response payload */
    maxContentType
}csvrContentType_e;

/***************************************************************************************************************
 * @brief Enumeration to set the HTTP response code.
 * @see <a href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Status#client_error_responses">HTTP response status codes</a> 
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

    csvrResponseMax
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
    csvrRequestType_e type; /* The HTTP request type. e.g. GET, POST, PUT, DELETE, etc. */
    csvrContentType_e contentType;  /* The body content type. */
    csvrHttpVersion_e httpVersion;  /* The incoming HTTP version */
    csvrConnectionType_e connectionType;
    int clientfd;           /* the client socket descriptor. */
    int contentLength;      /* the header content-length */
    char clientAddress[INET_ADDRSTRLEN];    /* the client IPv4 address */
    char host[100];         /* The host name of the client */
    char path[100];         /* The incoming request path */
    char serverName[100];   /* The copied server name */
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
    csvrContentType_e contentType;
    char *body;
    size_t contentLength;
}csvrResponse_t;

/***************************************************************************************************************
 * @brief Main structure to save all the csvr configuration, socket descriptor, and user path linked list.
 *        This structure will continue to be used as long as the user server is running.
 ***************************************************************************************************************/
typedef struct
{
    bool asyncFlag; /* To tell libcsvr that the server is running in async mode. */

    int sockfd;
    char serverName[100];
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
    void *userData;
    struct csvrPathUrl_t *next;
};

/***************************************************************************************************************
 * @brief Function to initialize an allocated csvrServer_t pointer which will be used for csvr procedure.
 *  
 * @param[in] port The user desired port on which the user wants the server to listen for incoming requests.
 * @return If success, it will return allocated pointer of csvrServer_t. Otherwise, it will return NULL.
 *         If it returns non NULL, the returned pointer must be freed using csvrShutdown function.
 ***************************************************************************************************************/
csvrServer_t *csvrInit(uint16_t port);

/************************************************************************************************************
 * @brief Function to start a new threads to handle incoming request.
 * 
 * @param[in] server Pointer to the csvrServer_t object which has been initialized using csvrInit.
 * @return csvrErrCode_e 
 *************************************************************************************************************/
csvrErrCode_e csvrServerStart(csvrServer_t *server);

/************************************************************************************************************
 * @brief This function will wait for any termination signal which has been initialized by csvrSignalInit.
 * 
 * @param server 
 * @return This function returns one of the following values:
 *          csvrSuccess,
 *          csvrNotAnError
 *************************************************************************************************************/
csvrErrCode_e csvrJoin(csvrServer_t *server);

/************************************************************************************************************
 * @brief Function to close the main socket that has been opened by csvrInit,
 *        releasing all the allocated memory in the csvrServer_t pointer variable, and
 *        releasing the csvrPathUrl_t linked list object pointer.
 * 
 * @param[in,out] server Pointer to the csvrServer_t object which has been initialized using csvrInit. 
 * @return This function always returns csvrSuccess 
 *************************************************************************************************************/
csvrErrCode_e csvrShutdown(csvrServer_t *input);

/***************************************************************************************************************
 * @brief Functions to read all the incoming data from newly created client socket from the listening procedure.
 * 
 * @param[in] server Pointer to the csvrServer_t object which has been initialized using csvrInit. 
 * @param[out] output 
 * @return This function returns one of the  following value : 
 *          csvrInvalidInput,
 *          csvrCannotListenSocket,
 *          csvrCannotAcceptSocket,
 *          csvrSuccess
 ***************************************************************************************************************/
csvrErrCode_e csvrRead(csvrServer_t *server, csvrRequest_t *output);

/************************************************************************************************************
 * @brief Function to close the client socket and releasing all the request and/or response resourced that 
 *        has been used.
 * 
 * @param[in,out] request Pointer to the user csvrRequest_t object. NULL allowed.
 * @param[in,out] response Pointer to the user csvrResponse_t object. NULL allowed.
 * @return This function always return csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrReadFinish(csvrRequest_t *request, csvrResponse_t *csvrResponseInput);

/************************************************************************************************************
 * @brief Function to send response
 * 
 * @param[in] request Pointer to request structure, must not NULL.
 * @param[in] response Pointer to response structure, must not NULL.
 * @return This function returns one of the following value : 
 *          csvrInvalidInput,
 *          csvrInvalidBody,
 *          csvrSystemFailure,
 *          csvrFailedSendData,
 *          csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrSendResponse(csvrRequest_t * request, csvrResponse_t *response);

/************************************************************************************************************
 * @brief Function to send HTTP error.
 * @note It is not a public API. The user must not use this function to avoid
 *       unexpected response.
 *
 * @param[in] request 
 * @param[in] code The desired HTTP response code which is defined in csvrHttpResponseCode_e.
 * @param[in] desc The HTTP reseponse code descriptions.
 * 
 * @return This function returns one of the following value : 
 *          csvrInvalidInput,
 *          csvrFailedSendData,
 *          csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrSendResponseError(csvrRequest_t * request, csvrHttpResponseCode_e code, char*desc);

/***************************************************************************************************************
 * @brief Function to set custom server name. Will be used in the response Header payload.
 * 
 * @param[in,out] server Pointer to the csvrServer_t object which has been initialized using csvrInit. 
 * @param[in] serverName The desired server name. This input accept valist type.
 * 
 * @return This function returns one of the following value : 
 *          csvrInvalidInput,
 *          csvrNotAnError,
 *          csvrSystemFailure,
 *          csvrSuccess
 ***************************************************************************************************************/
csvrErrCode_e csvrSetCustomServerName(csvrServer_t *server, char *serverName,...);

/************************************************************************************************************
 * @brief Function to create custom header response.
 * 
 * @param[in,out] input pointer to csvrResponse_t variable.
 * @param[in] key the header key. Example : "Accept",  "Content-Type", "your-own-header",etc.
 * @param[in] value the value of the header key. Example : "application/json", "text/html", "Your own header key" etc.
 * 
 * @return This function returns one of the following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value);

/************************************************************************************************************
 * @brief Function to add content payload to the response body.
 * 
 * @param[in,out] input The user pointer to csvrResponse_t object.
 * @param[in] contentType The body payload content type.
 * @param[in] content The user body payload. This input accept valist type.
 *
 * @return This function returns one of the following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrAddContent(csvrResponse_t*input, csvrContentType_e contentType, char *content,...);

/************************************************************************************************************
 * @brief Function to add API request
 * 
 * @param[in,out] server Pointer to the csvrServer_t object which has been initialized using csvrInit. 
 * @param[in] path The URI path. e.g. : "/", "/time", "/api/v1", etc.
 * @param[in] type The type of the desired URI which is defined in csvrRequestType_e
 * @param[in] callbackFunction The callback function. All the pointer to the callback function wil be saved in
 *                             the csvrPathUrl_t->callbackFunction object. It only needs two argument:
 *                             (1) the pointer to csvrRequest_t structure, and
 *                             (2) the user data object which will be saved in (void*) type pointer.
 * @param[in] userData Pointer to the user callbackFunction argument data. This pointer will be given as 
 *                     the input of the second argument in the callback function.
 * 
 * @return This function returns one of the following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrAddPath(csvrServer_t *server, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrRequest_t *, void *), void *userData);

/************************************************************************************************************
 * @brief Function to get current total client connection socket which is saved in the _totalConnectionNow variable.
 *        The value will be reset if the application is restarted.
 * 
 * @return This function will return the total of current socket client connections.
 *************************************************************************************************************/
long csvrGetTotalConnection();

#ifdef __cplusplus
}
#endif
#endif