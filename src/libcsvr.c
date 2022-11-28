#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "libcsvr.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME CSVR_NAME
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION CSVR_VERSION
#endif

#define HEADER_CONTENT_LENGTH_KEY "Content-Length: "
#define HEADER_CONTENT_TYPE_KEY   "Content-Type: "
#define HEADER_SEPARATOR          "\x0D\x0A"
#define BODY_SEPARATOR            "\x0D\x0A\x0D\x0A"
#define DEFAULT_MESSAGE_ALLOCATION 3
#define DEFAULT_SERVER_NAME       CSVR_NAME"-"CSVR_VERSION

#define MINIMUM_REQUEST_TYPE_LENGTH strlen("PUT")
#define MAXIMUM_REQUEST_TYPE_LENGTH strlen("DELETE")

#ifndef FREE
#define FREE(ptr) if(ptr != NULL) {free(ptr);ptr = NULL;}
#endif

static int totalConnectionAllowed = 5;
static pthread_t serverThreads;
pthread_mutex_t lockRead = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    void *userData;
    csvrServer_t *server;
    csvrRequest_t request;
}csvrThreadsData_t;

size_t lengthTypeTranslator[csvrTypeMax] = 
{
    [csvrTypeNotKnown ] = 0,
    [csvrTypeGet      ] = 3,
    [csvrTypePut      ] = 3,
    [csvrTypePost     ] = 4,
    [csvrTypeHead     ] = 4,
    [csvrTypeDelete   ] = 6,
};

char *typeStringTranslator[] = 
{
    [csvrTypeNotKnown ] = "",
    [csvrTypeGet      ] = "GET ",
    [csvrTypePut      ] = "PUT ",
    [csvrTypePost     ] = "POST ",
    [csvrTypeHead     ] = "HEAD ",
    [csvrTypeDelete   ] = "DELETE ",
};

static csvrRequestType_e getRequestType(char*header)
{
    csvrRequestType_e type = csvrTypeNotKnown;
    if(!memcmp("POST", header, 4))
    {
        type = csvrTypePost;
    }
    else if(!memcmp("GET", header, 3))
    {
        type = csvrTypeGet;
    }
    else if(!memcmp("HEAD", header, 4))
    {
        type = csvrTypeHead;
    }
    else if(!memcmp("DELETE", header, 6))
    {
        type = csvrTypeDelete;
    }
    else if(!memcmp("PUT", header, 3))
    {
        type = csvrTypePut;
    }
    return type;
}

static int getContentLength(char*header, size_t headerLen)
{
    if(header == NULL)
    {
        return csvrInvalidInput;
    }

    int contentLength = 0;
    char *line = NULL;
    char buffer[100];
    size_t index = 0;
    size_t lastIndex = 0;
    while(index < headerLen)
    {
        /* Get 0x0D 0x0A */
        if(!memcmp(header+index, HEADER_SEPARATOR, strlen(HEADER_SEPARATOR)))
        {
            size_t lengthLine = index - lastIndex;
            line = calloc((lengthLine + 1), sizeof(char));
            if(line)
            {
                memset(line, 0, (lengthLine + 1));
                memcpy(line, header + lastIndex, lengthLine);
                if(!memcmp(line,HEADER_CONTENT_LENGTH_KEY, strlen(HEADER_CONTENT_LENGTH_KEY)))
                {
                    memset(buffer, 0, sizeof(buffer));
                    snprintf(buffer,sizeof(buffer), "%s", line + strlen(HEADER_CONTENT_LENGTH_KEY));
                    contentLength = atoi(buffer);
                    break;
                }
                FREE(line);
            }
            lastIndex = index + strlen(HEADER_SEPARATOR);
        }
        index++;
    }
    FREE(line);
    return contentLength;
}

static csvrContentType_e getContentType(char*header)
{
    if(header == NULL)
    {
        return noContentType;
    }

    csvrContentType_e contentType = textHtml;
    char *line = NULL;
    char buffer[100];
    size_t index = 0;
    size_t lastIndex = 0;
    size_t headerLen = strlen(header);
    while(index < headerLen)
    {
        /* Get 0x0D 0x0A */
        if(!memcmp(header+index, HEADER_SEPARATOR, strlen(HEADER_SEPARATOR)))
        {
            size_t lengthLine = index - lastIndex;
            line = calloc((lengthLine + 1), sizeof(char));
            if(line)
            {
                memset(line, 0, (lengthLine + 1));
                memcpy(line, header + lastIndex, lengthLine);
                if(!memcmp(line,HEADER_CONTENT_TYPE_KEY, strlen(HEADER_CONTENT_TYPE_KEY)))
                {
                    memset(buffer, 0, sizeof(buffer));
                    snprintf(buffer,sizeof(buffer), "%s", line + strlen(HEADER_CONTENT_TYPE_KEY));
                    size_t bufferlen = strlen(buffer);
                    size_t i = 0;
                    for(;i < bufferlen; i++)
                    {
                        char c = buffer[i];
                        buffer[i] = (char)tolower(c);
                    }
                    if(!memcmp(buffer, "application/json", strlen("application/json")))
                    {
                        contentType = applicationJson;
                    }
                    break;
                }
                FREE(line);
            }
            lastIndex = index + strlen(HEADER_SEPARATOR);
        }
        index++;
    }
    FREE(line);

    return contentType;
}

static csvrErrCode_e getHeaderKeyValue(char*header, char*key, char *dest, size_t maxlen)
{
    if(header == NULL || key == NULL || dest == NULL)
    {
        return csvrInvalidInput;
    }

    size_t ret = 0;
    char *line = NULL;
    char buffer[100];
    size_t index = 0;
    size_t lastIndex = 0;
    size_t headerLen = strlen(header);
    while(index < headerLen)
    {
        /* Get 0x0D 0x0A */
        if(!memcmp(header+index, HEADER_SEPARATOR, strlen(HEADER_SEPARATOR)))
        {
            size_t lengthLine = index - lastIndex;
            line = calloc((lengthLine + 1), sizeof(char));
            if(line)
            {
                memset(line, 0, (lengthLine + 1));
                memcpy(line, header + lastIndex, lengthLine);
                /* Check if key found */
                if(!memcmp(line, key, strlen(key)))
                {
                    memset(buffer, 0, maxlen);
                    snprintf(dest, maxlen, "%s", line + strlen(key) + strlen(": "));
                    ret = csvrSuccess;
                    break;
                }
                FREE(line);
            }
            lastIndex = index + strlen(HEADER_SEPARATOR);
        }
        index++;
    }
    FREE(line);

    return ret;
}

static csvrErrCode_e getRequestUriPath(char*header, char*dest, size_t maxlen)
{
    if(header == NULL || dest == NULL)
    {
        return csvrInvalidInput;
    }

    csvrErrCode_e ret = csvrSystemFailure;
    char *buffer = NULL;
    size_t index = 0;
    size_t firstSpaceIndex = 0;
    size_t headerLen = strlen(header);
    while(index < headerLen)
    {
        /* Get the first space */
        if((header[index - 1] == ' ') && firstSpaceIndex == 0 && index > 0)
        {
            firstSpaceIndex = index;
        }

        if(!memcmp(header + index, " HTTP", strlen(" HTTP")))
        {
            /* Get The first ' HTTP' */
            size_t lengthLine = index - firstSpaceIndex + 1;
            buffer = calloc(lengthLine, sizeof(char));
            if(buffer == NULL)
            {
                break;
            }

            memset(buffer, 0, lengthLine);
            memcpy(buffer, header + firstSpaceIndex, lengthLine - 1);
            
            memset(dest,0,maxlen);
            snprintf(dest, maxlen, "%s", buffer);
            free(buffer);
            ret = csvrSuccess;
            break;
        }
        index++;
    }

    if(index == headerLen)
    {
        ret = csvrInvalidHeader;
    }
    return ret;
}

static csvrHttpVersion_e getHttpVersion(char*header)
{
    csvrHttpVersion_e version = http1_0;
    size_t len = 0;
    char buffer[10];

    char *ptr1 = NULL;
    char *ptr2 = NULL;
    ptr1 = strstr(header, "HTTP/");
    if(ptr1)
    {
        ptr2 = strstr(ptr1, HEADER_SEPARATOR);
        len = (size_t)(ptr2 - ptr1);
        char *res = (char*)malloc(sizeof(char)*(len+1));
        if(res)
        {
            strncpy(res, ptr1, len);
            res[len] = '\0';
            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, res + strlen("HTTP/"), strlen(res) - strlen("HTTP/"));
            if(!memcmp(buffer, "1.1", 3))
            {
                version = http1_1;
            }
        }
        FREE(res);
    }

    return version;
}

/**
 * @brief 
 * 
 * @param[in] input 
 * @param[in] port 
 * @return csvrErrCode_e 
 */
csvrErrCode_e csvrInit(csvrServer_t *input, uint16_t port)
{
    if(input == NULL)
    {
        return csvrInvalidInput;
    }

    input->port  = port;
    input->sockfd = -1;
    input->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (input->sockfd == -1)
    {
        return csvrCannotCreateSocket;
    }

    /**< 2. Assign IP, dan PORT to be used for server *///
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(input->port);

    /**< 3. Binding newly created socket to given IP and verification *///
    int tryTimes = 0;
    while (bind(input->sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        if(tryTimes > 10)
        {
            close(input->sockfd);
            input->sockfd = -1;
            return csvrCannotBindingSocket;
        }
        tryTimes++;
        sleep(1);
    }

    size_t lenServerName = strlen(CSVR_NAME) + strlen(CSVR_VERSION) + 1;
    input->serverName = calloc(lenServerName + 1, sizeof(char));
    memset(input->serverName, 0, lenServerName + 1);
    snprintf(input->serverName, lenServerName + 1, "%s-%s", (char*)CSVR_NAME, (char*)CSVR_VERSION);

    input->path = NULL;
    return csvrSuccess;
}

/**
 * @brief 
 * 
 * @param[in,out] input 
 * @param[in] serverName 
 * 
 * @return csvrErrCode_e 
 */
csvrErrCode_e csvrSetCustomServerName(csvrServer_t *input, char *serverName, ...)
{
    if(input == NULL || serverName == NULL)
    {
        return csvrInvalidInput;
    }

    char *temp = NULL;
    va_list aptr;
    va_start(aptr, serverName);
    int ret = vasprintf(&temp, serverName, aptr);
    va_end(aptr);
    if(ret == -1)
    {
        return -1;
    }

    if(strlen(temp) == 0)
    {
        FREE(temp);
        return csvrNotAnError;
    }

    FREE(input->serverName);
    input->serverName = calloc(strlen(temp) + 1, sizeof(char));
    memset(input->serverName, 0, strlen(temp) + 1);
    snprintf(input->serverName, strlen(temp), "%s", temp);
    FREE(temp);
    if(input->serverName == NULL)
    {
        return csvrSystemFailure;
    }

    return csvrSuccess;
}

static csvrErrCode_e csvrClientReader(csvrRequest_t *output)
{
    size_t lenMessage    = 0;
    ssize_t retval       = 0;
    char character       = 0;
    char *message        = NULL;
    bool flagReadBody    = false;
    int contentLength    = 0;
    size_t indexBody     = 0;

    /* Initialize */
    output->header  = NULL;
    output->content = NULL;
    output->message = NULL;
    message = malloc(DEFAULT_MESSAGE_ALLOCATION*sizeof(char));
    memset(message, 0, DEFAULT_MESSAGE_ALLOCATION);

    while(1)
    {
        /*3.  Read the client socket until EOF, or 0 */
        retval = read(output->clientfd, &character, 1);
        if(retval == 0) break;
        else if(retval < 0)
        {
            break;
        }

        message[lenMessage] = character;
        lenMessage += (size_t)retval;
        message     = realloc(message, (size_t)(DEFAULT_MESSAGE_ALLOCATION+lenMessage));

        /* Finish read header */
        if(!memcmp(message + lenMessage - strlen(BODY_SEPARATOR), BODY_SEPARATOR, strlen(BODY_SEPARATOR)))
        {
            output->header = calloc(lenMessage + 1, sizeof(char));
            memset(output->header, 0, lenMessage + 1);
            snprintf(output->header, lenMessage, "%s", message);

            /* Get request type*/
            if(output->type == csvrTypeNotKnown)
            {
                output->type = getRequestType(message);
            }
            
            output->httpVersion = getHttpVersion(message);
            getHeaderKeyValue(output->header, "Host", output->host, sizeof(output->host));
            getRequestUriPath(output->header, output->path, sizeof(output->path));

            if(output->type == csvrTypePost)
            {
                /* Parse data from header */
                if(flagReadBody == false)
                {
                    flagReadBody            = true;
                    indexBody               = lenMessage;
                    output->contentType     = getContentType(output->header);
                    contentLength           = getContentLength(output->header, lenMessage);
                    output->contentLength   = getContentLength(output->header, lenMessage);
                }
                continue;
            }
            break;
        }
        else
        {
            /* Read the payload until it reach the maximum contentLength */
            if(flagReadBody == true)
            {
                contentLength--;
                if(contentLength <= 0)
                {
                    output->content = calloc(output->contentLength + 1, sizeof(char));
                    memset(output->content, 0, output->contentLength + 1);
                    memcpy(output->content, message + indexBody, output->contentLength);
                    break;
                }
            }
        }
    }

    if(retval <= 0)
    {
        printf("Read failed : %s\n",strerror(errno));
        close(output->clientfd);
        FREE(message);
        FREE(output->header);
        FREE(output->content);
        return csvrFailedReadSocket;
    }

    output->message = calloc(lenMessage + 1, sizeof(char));
    memset(output->message, 0, lenMessage + 1);
    snprintf(output->message, lenMessage, "%s", message);
    FREE(message);
    return csvrSuccess;
}

/**
 * @brief 
 * 
 * @param[in] input 
 * @param[out] output 
 * @return csvrErrCode_e 
 */
csvrErrCode_e csvrRead(csvrServer_t *input, csvrRequest_t *output)
{
    if(input == NULL || output == NULL)
    {
        return csvrInvalidInput;
    }

    csvrErrCode_e ret = csvrSystemFailure;
    pthread_mutex_lock(&lockRead);
    do
    {
        /* 1. Listen to socket */
        if (listen(input->sockfd, totalConnectionAllowed) != 0)
        {
            ret = csvrCannotListenSocket;
            break;
        }

        struct sockaddr_in client;  
        memset(&client,0,sizeof(struct sockaddr_in));

        /* 2. Get the socket configuration to get the client address */
        socklen_t peerAddrSize = sizeof(client);
        output->clientfd = -1;
        output->clientfd = accept(input->sockfd, (struct sockaddr*)&client, &peerAddrSize);
        if (output->clientfd < 0)
        {
            ret = csvrCannotAcceptSocket;
            break;
        }

        struct in_addr ipAddr = client.sin_addr;
        memset(output->clientAddress,0,sizeof(output->clientAddress));
        inet_ntop(AF_INET, &ipAddr, output->clientAddress, INET_ADDRSTRLEN );

        ret = csvrClientReader(output);
        if(ret != csvrSuccess)
        {
            continue;
        }
        #if 0
        size_t lenMessage    = 0;
        ssize_t retval       = 0;
        char character       = 0;
        char *message        = NULL;
        bool flagReadBody    = false;
        int contentLength = 0;
        size_t indexBody     = 0;

        /* Initialize */
        output->header  = NULL;
        output->content = NULL;
        output->message = NULL;
        message = malloc(DEFAULT_MESSAGE_ALLOCATION*sizeof(char));
        memset(message, 0, DEFAULT_MESSAGE_ALLOCATION);

        while(1)
        {
            /*3.  Read the client socket until EOF, or 0 */
            retval = read(output->clientfd, &character, 1);
            if(retval == 0) break;
            else if(retval < 0)
            {
                break;
            }

            message[lenMessage] = character;
            lenMessage += (size_t)retval;
            message     = realloc(message, (size_t)(DEFAULT_MESSAGE_ALLOCATION+lenMessage));

            /* Finish read header */
            if(!memcmp(message + lenMessage - strlen(BODY_SEPARATOR), BODY_SEPARATOR, strlen(BODY_SEPARATOR)))
            {
                output->header = calloc(lenMessage + 1, sizeof(char));
                memset(output->header, 0, lenMessage + 1);
                snprintf(output->header, lenMessage, "%s", message);

                /* Get request type*/
                if(output->type == csvrTypeNotKnown)
                {
                    output->type = getRequestType(message);
                }
                
                output->httpVersion = getHttpVersion(message);
                getHeaderKeyValue(output->header, "Host", output->host, sizeof(output->host));
                getRequestUriPath(output->header, output->path, sizeof(output->path));

                if(output->type == csvrTypePost)
                {
                    /* Parse data from header */
                    if(flagReadBody == false)
                    {
                        flagReadBody            = true;
                        indexBody               = lenMessage;
                        output->contentType     = getContentType(output->header);
                        contentLength           = getContentLength(output->header, lenMessage);
                        output->contentLength   = getContentLength(output->header, lenMessage);
                    }
                    continue;
                }
                break;
            }
            else
            {
                /* Read the payload until it reach the maximum contentLength */
                if(flagReadBody == true)
                {
                    contentLength--;
                    if(contentLength <= 0)
                    {
                        output->content = calloc(contentLength + 1, sizeof(char));
                        memset(output->content, 0, contentLength + 1);
                        memcpy(output->content, message + indexBody, contentLength);
                        break;
                    }
                }
            }
        }

        if(retval <= 0)
        {
            printf("Read failed : %s\n",strerror(errno));
            close(output->clientfd);
            FREE(message);
            FREE(output->header);
            FREE(output->content);
            continue;
        }

        output->message = calloc(strlen(message) + 1, sizeof(char));
        memset(output->message, 0, strlen(message) + 1);
        snprintf(output->message, strlen(message), "%s", message);
        FREE(message);
        ret = csvrSuccess;
        #endif
    }while(0);
    pthread_mutex_unlock(&lockRead);
    return ret;
}

struct csvrPathUrl_t *csvrSearchUri(struct csvrPathUrl_t *input, char *path)
{
    struct csvrPathUrl_t * current = NULL;
    if(input && path)
    {
        current = input;
        while(current)
        {
            if(!memcmp(current->name, path, strlen(path)))
            {
                return current;
            }
            current = current->next;
        }
    }

    return current;
}

static void *csvrClientReaderThreads(void *arg)
{
    csvrThreadsData_t *data = (csvrThreadsData_t*)arg;
    csvrErrCode_e ret = csvrSuccess;
    ret = csvrClientReader(&(data->request));
    /* if success, do the procedure based on the path */
    if(ret == csvrSuccess)
    {
        struct csvrPathUrl_t * path = NULL;
        path = csvrSearchUri(data->server->path, data->request.path);
        (*path->callbackFunction)(data->server, &(data->request), data->userData);
    }

    pthread_exit(NULL);
}

static void *csvrAsyncronousThreads(void * arg)
{
    csvrThreadsData_t *threadsData = (csvrThreadsData_t *)arg;

    threadsData->server->asyncFlag = true;

    while(1)
    {
        csvrRequest_t request;
        memset(&request,0,sizeof(csvrRequest_t));

        /* 1. Listen to socket */
        if (listen(threadsData->server->sockfd, totalConnectionAllowed) != 0)
        {
            continue;
        }

        struct sockaddr_in client;  
        memset(&client,0,sizeof(struct sockaddr_in));

        /* 2. Get the socket configuration to get the client address */
        socklen_t peerAddrSize = sizeof(client);
        request.clientfd = -1;
        request.clientfd = accept(threadsData->server->sockfd, (struct sockaddr*)&client, &peerAddrSize);
        if (request.clientfd < 0)
        {
            continue;
        }

        struct in_addr ipAddr = client.sin_addr;
        memset(request.clientAddress,0,sizeof(request.clientAddress));
        inet_ntop(AF_INET, &ipAddr, request.clientAddress, INET_ADDRSTRLEN );

        pthread_t threadClient;
        memset(&(threadsData->request), 0, sizeof(csvrRequest_t));
        memcpy(&(threadsData->request), &request, sizeof(csvrRequest_t));

        int status = pthread_create(&threadClient, NULL, csvrClientReaderThreads, (void*)&threadsData);
        /* If success, detach the threads */
        if(status == 0)
        {
            pthread_detach(threadClient);
        }
        /* If failed, send response and close the connection */
        else
        {
            csvrResponse_t response;
            memset(&response,0,sizeof(csvrResponse_t));
            char * jsonData = "{\"status\":500,\"descriptions\":\"Internal server error\"}\n";
            csvrAddContent(&response, jsonData);
            csvrSendResponse(threadsData->server, &request, &response);
            csvrReadFinish(&request, &response);
        }
        usleep(100000);
    }
    pthread_exit(NULL);
}
/**
 * @brief If this function is called, it will spawn threads to handle incoming request
 * 
 * @param[in] input 
 * @param[in] output 
 * @return csvrErrCode_e 
 */
csvrErrCode_e csvrServerStart(csvrServer_t *input, void *userData)
{
    if(input == NULL)
    {
        return csvrInvalidInput;
    }

    csvrThreadsData_t threadsData;
    memset(&threadsData, 0, sizeof(csvrThreadsData_t));
    threadsData.server = input;
    threadsData.userData = userData;
    int status = pthread_create(&serverThreads,NULL,csvrAsyncronousThreads,(void*)&threadsData);
    if(status != 0)
    {
        return csvrSystemFailure;
    }
    return csvrSuccess;
}

/**
 * @brief 
 * 
 * @param[in,out] input 
 * @param[in] request 
 * @param[in] response 
 * @return csvrErrCode_e 
 */
csvrErrCode_e csvrSendResponse(csvrServer_t *input, csvrRequest_t * request, csvrResponse_t *response)
{
    if(input == NULL || response == NULL)
    {
        return csvrInvalidInput;
    }

    if(response->body == NULL)
    {
        return csvrInvalidBody;
    }

    csvrErrCode_e ret = csvrSuccess;
    char dtime[100];
    memset(dtime,0,sizeof(dtime));
    time_t now = time(0);
    struct tm *tm = gmtime(&now);
    strftime(dtime, sizeof(dtime), "%a, %d %b %Y %H:%M:%S %Z", tm);
    
    const char *template = "HTTP/1.1 200 OK\r\n"
                            "Server: %s\r\n"
                            "Accept-Ranges: none\r\n"
                            "Vary: Accept-Encoding\r\n"
                            "Last-Modified: %s\r\n"
                            "Connection: closed\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %u\r\n"
                            "\r\n"
                            "%s"
                            ;

    char *message = NULL;
    size_t lenBody = strlen(response->body);
    size_t lenMessage = strlen(template) + strlen(dtime) + lenBody + strlen(input->serverName) + 10;

    message = malloc((lenMessage)*sizeof(char));
    memset(message,0,lenMessage*sizeof(char));
    
    snprintf(message, lenMessage,
        template, 
        input->serverName,
        dtime, 
        lenBody,
        response->body);

    ssize_t sendStatus = 0;
    sendStatus = send(request->clientfd, message, strlen(message), 0);
    close(request->clientfd);

    if(sendStatus > 0)
    {
        ret = csvrSuccess;
    }
    else
    {
        ret = csvrFailedSendData;
    }

    FREE(message);
    return ret;
}

csvrErrCode_e csvrReadFinish(csvrRequest_t *input, csvrResponse_t *response)
{
    if(input == NULL)
    {
        return csvrInvalidInput;
    }

    FREE(input->message);
    FREE(input->header);
    FREE(input->content);
    memset(input, 0, sizeof(csvrRequest_t));

    /* Free header if any */
    int i = 0;
    for(;i<response->header.total;i++)
    {
        FREE(response->header.data[i]);
    }
    FREE(response->header.data);

    /* Free body */
    FREE(response->body);
    memset(response, 0, sizeof(csvrResponse_t));

    return csvrSuccess;
}

csvrErrCode_e csvrShutdown(csvrServer_t *input)
{
    if(input == NULL)
    {
        return csvrInvalidInput;
    }

    if(input->asyncFlag)
    {
        pthread_cancel(serverThreads);
        pthread_join(serverThreads, NULL);
    }

    FREE(input->serverName);
    if(input->sockfd) close(input->sockfd);

    if(input->path)
    {
        struct csvrPathUrl_t* current = (input->path);
        struct csvrPathUrl_t* next = NULL;
        while (1)
        {
            if(current != NULL)
            {
                next = current->next;
                FREE(current->name);
                FREE(current);
            }
            else break;
            current = next;
        }
    
        input->path = NULL;
    }

    return csvrSuccess;
}

/**
 * @brief Function to create custom he    if(isRunning) pthread_cancel(thrServer);
ader response
 * 
 * @param[in,out] input
 * @param[in] key
 * @param[in] value
 * 
 * @return This function will return following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 */
csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value)
{
    if(input == NULL || key == NULL || value == NULL)
    {
        return csvrInvalidInput;
    }

    char *buffer = NULL;
    size_t lenBuffer = strlen(key) + strlen(": ") + strlen(value) + strlen(HEADER_SEPARATOR) + 1;
    buffer = malloc(lenBuffer*sizeof(char));
    if(buffer == NULL)
    {
        return csvrSystemFailure;
    }

    memset(buffer,0,lenBuffer);
    snprintf(buffer, lenBuffer, "%s: %s%s",key,value,HEADER_SEPARATOR);

    char **newHeader = NULL;
    input->header.total++;
    newHeader = calloc(input->header.total, sizeof(char*));
    if(newHeader == NULL)
    {
        FREE(buffer);
        return csvrSystemFailure;
    }

    size_t i = 0;
    for(;i < (input->header.total - 1);i++)
    {
        newHeader[i] = calloc(strlen(input->header.data[i]) + 1, sizeof(char));
        memset(newHeader[i], 0, strlen(input->header.data[i]) + 1);
        snprintf(newHeader[i],  strlen(input->header.data[i]), "%s", input->header.data[i]);
        FREE(input->header.data[i]);
    }
    newHeader[input->header.total - 1] = calloc(strlen(buffer) + 1, sizeof(char));
    memset(newHeader[input->header.total - 1], 0, strlen(buffer) + 1);
    snprintf(newHeader[input->header.total - 1], strlen(buffer), "%s", buffer);
    FREE(buffer);

    FREE(input->header.data)
    input->header.data = newHeader;
    return csvrSuccess;
}

/**
 * @brief Function to add API request
 * 
 * @param[in,out] input
 * @param[in] path : The url path. e.g. : "/", "/time", "/api/v1", etc.
 * @param[in] type
 * @param[in] callbackFunction 
 * 
 * @return This function will return following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 */
csvrErrCode_e csvrAddPath(csvrServer_t *input, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrServer_t*, csvrRequest_t *, void *))
{
    if(input == NULL || path == NULL || (*callbackFunction) == NULL)
    {
        return csvrInvalidInput;
    }

    /* Handle if server not initialize yet */
    if(input->sockfd == -1)
    {
        return csvrNotAnError;
    }

    /* Search if URI already exists */
    struct csvrPathUrl_t * current = NULL;
    if(input->path)
    {
        current = input->path;
        while(current)
        {
            if(!memcmp(current->name, path, strlen(path)))
            {
                return csvrUriAlreadyExists;
            }
            current = current->next;
        }
    }

    struct csvrPathUrl_t * newPath = NULL;
    newPath = (struct csvrPathUrl_t*)malloc(sizeof(struct csvrPathUrl_t));
    if(newPath == NULL)
    {
        return csvrSystemFailure;
    }

    newPath->type = type;
    newPath->name = strdup(path);
    newPath->callbackFunction = (*callbackFunction);
    newPath->next = input->path;
	input->path   = newPath;

    return csvrSuccess;
}

/**
 * @brief Function to add content to body response
 * 
 * @param[in,out] input 
 * @param[in] content This input is valist type
 *
 * @return This function will return following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 */
csvrErrCode_e csvrAddContent(csvrResponse_t *input, char *content, ...)
{
    if(input == NULL || content == NULL)
    {
        return csvrInvalidInput;
    }

    char *bodyTemp = NULL;
    va_list aptr;
    va_start(aptr, content);
    int ret = vasprintf(&bodyTemp, content, aptr);
    va_end(aptr);
    if(ret == -1)
    {
        return -1;
    }

    FREE(input->body);
    input->body = calloc(strlen(bodyTemp) + 1, sizeof(char));
    memset(input->body, 0, strlen(bodyTemp) + 1);
    snprintf(input->body, strlen(bodyTemp), "%s", bodyTemp);
    FREE(bodyTemp);
    return csvrSuccess;
}

#ifdef TEST

/**
 * @brief Compile with :
 * 
 *    $ gcc src/libcsvr.c -Iinclude/ -o test -D_GNU_SOURCE -DTEST
 *    $ ./test
 * 
 */
int main()
{
    char header[] =
    {
        0x50,0x4F,0x53,0x54,0x20,0x2F,0x20,0x48,0x54,0x54,0x50,0x2F,0x31,0x2E,0x31,0x0D,
        0x0A,0x48,0x6F,0x73,0x74,0x3A,0x20,0x6C,0x6F,0x63,0x61,0x6C,0x68,0x6F,0x73,0x74,
        0x3A,0x39,0x30,0x30,0x30,0x0D,0x0A,0x55,0x73,0x65,0x72,0x2D,0x41,0x67,0x65,0x6E,
        0x74,0x3A,0x20,0x63,0x75,0x72,0x6C,0x2F,0x37,0x2E,0x36,0x38,0x2E,0x30,0x0D,0x0A,
        0x41,0x63,0x63,0x65,0x70,0x74,0x3A,0x20,0x2A,0x2F,0x2A,0x0D,0x0A,0x43,0x6F,0x6E,
        0x74,0x65,0x6E,0x74,0x2D,0x54,0x79,0x70,0x65,0x3A,0x20,0x61,0x70,0x70,0x6C,0x69,
        0x63,0x61,0x74,0x69,0x6F,0x6E,0x2F,0x6A,0x73,0x6F,0x6E,0x0D,0x0A,0x43,0x6F,0x6E,
        0x74,0x65,0x6E,0x74,0x2D,0x4C,0x65,0x6E,0x67,0x74,0x68,0x3A,0x20,0x31,0x39,0x0D,
        0x0A,0x0D,0x0A
    };

    char path[100];
    memset(path, 0, sizeof(path));
    getRequestUriPath(header,path,sizeof(path));

    int http = getHttpVersion(header);
    printf("Http version: %d\n",http);
    printf("Path: %s\n",path);
    printf("Type request: %s\n",typeStringTranslator[getRequestType(header)]);
    size_t ret = getContentLength(header, strlen(header));
    printf("Result: %lu\n",ret);
    printf("Servername %s\n",DEFAULT_SERVER_NAME);

    csvrResponse_t response;
    csvrRequest_t request;
    memset(&request, 0, sizeof(csvrRequest_t));
    memset(&response, 0, sizeof(csvrResponse_t));
    csvrAddCustomHeader(&response, "session-uuid","a58ae080-6efc-11ed-b9d7-0800274047c4");
    csvrAddCustomHeader(&response, "sender-name","Ergi");
    csvrAddCustomHeader(&response, "sender-address","Jakarta");
    int i = 0;
    for(;i < response.header.total;i++)
    {
        printf("[%d] %s\n",i,response.header.data[i]);
    }
    csvrReadFinish(&request,&response);
    return 0;
}
#endif