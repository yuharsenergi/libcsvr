/********************************************************************************
 * @file libcsvr.c
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
#include "libcsvr_response.h"
#include "libcsvr_signal.h"

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
#define GET_ERRNO() {printf("errno : (%d) %s\n", errno,strerror(errno));}

#define MINIMUM_REQUEST_TYPE_LENGTH strlen("PUT")
#define MAXIMUM_REQUEST_TYPE_LENGTH strlen("DELETE")

#ifndef FREE
#define FREE(ptr) if(ptr != NULL) {free(ptr);ptr = NULL;}
#endif

static long totalConnectionNow = 0;
static int totalConnectionAllowed = 100;
static pthread_t serverThreads;
static sem_t waitThread;
static pthread_mutex_t lockRead = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockCount = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lockSearchPath = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    void *userData;
    csvrServer_t *server;
    csvrRequest_t *request;
}csvrThreadsData_t;

static size_t lengthTypeTranslator[csvrTypeMax] = 
{
    [csvrTypeNotKnown ] = 0,
    [csvrTypeGet      ] = 3,
    [csvrTypePut      ] = 3,
    [csvrTypePost     ] = 4,
    [csvrTypeHead     ] = 4,
    [csvrTypeDelete   ] = 6,
    [csvrTypeUnknwon  ] = 0,
};

static char *typeStringTranslator[] = 
{
    [csvrTypeNotKnown ] = "",
    [csvrTypeGet      ] = "GET ",
    [csvrTypePut      ] = "PUT ",
    [csvrTypePost     ] = "POST",
    [csvrTypeHead     ] = "HEAD",
    [csvrTypeDelete   ] = "DELETE",
    [csvrTypeUnknwon  ] = "",
};

static void csvrIncreaseConnectionCounter()
{
    pthread_mutex_lock(&lockCount);
    totalConnectionNow++;
    pthread_mutex_unlock(&lockCount);
}

static void csvrDecreaseConnectionCounter()
{
    pthread_mutex_lock(&lockCount);
    totalConnectionNow--;
    pthread_mutex_unlock(&lockCount);
}

static csvrRequestType_e getRequestType(char*header)
{
    csvrRequestType_e type = csvrTypeUnknwon;
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
        return 0;
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

    csvrContentType_e contentType = noContentType;
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
                    else if(!memcmp(buffer, "text/html", strlen("text/html")))
                    {
                        contentType = textHtml;
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

/***************************************************************************************************************
 * @brief Function to inintialize an allocated csvrServer_t pointer which will be used for csvr procedure.
 *  
 * @param[in] port 
 * @return If success, it will return allocated pointer of csvrServer_t. Otherwise, it will return NULL.
 *         If it returns non NULL, the returned pointer must be freed using csvrShutdown function.
 ***************************************************************************************************************/
csvrServer_t *csvrInit(uint16_t port)
{
    csvrServer_t * server = NULL;
    server = calloc(1, sizeof(csvrServer_t));
    if(server == NULL)
    {
        printf("[ERROR] Cannot allocate server object memory\n");
        return NULL;
    }

    memset(server, 0, sizeof(csvrServer_t));
    server->port  = port;
    server->sockfd = -1;
    server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->sockfd == -1)
    {
        printf("[ERROR] Cannot create socket: ");GET_ERRNO();
        free(server);
        return NULL;
    }

    int enable = 1;
    if(setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
    {
        printf("[ERROR] Cannot setsockopt socket: ");GET_ERRNO();
        free(server);
        return NULL;
    }

    /**< 2. Assign IP, dan PORT to be used for server *///
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(server->port);

    /**< 3. Binding newly created socket to given IP and verification *///
    int tryTimes = 0;
    while (bind(server->sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        if(tryTimes > 10)
        {
            printf("[ERROR] Cannot create socket: ");GET_ERRNO();
            close(server->sockfd);
            free(server);
            return NULL;
        }
        printf("[DEBUG] Try %d binding socket failed: ", tryTimes);GET_ERRNO();
        tryTimes++;
        sleep(1);
    }

    FREE(server->serverName);

    size_t lenServerName = strlen(CSVR_NAME) + strlen(CSVR_VERSION) + 1;
    server->serverName = calloc(lenServerName + 1, sizeof(char));
    memset(server->serverName, 0, lenServerName + 1);
    snprintf(server->serverName, lenServerName + 1, "%s-%s", (char*)CSVR_NAME, (char*)CSVR_VERSION);

    server->path = NULL;
    return server;
}

/***************************************************************************************************************
 * @brief 
 * 
 * @param[in,out] input 
 * @param[in] serverName 
 * 
 * @return csvrErrCode_e 
 ***************************************************************************************************************/
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
    snprintf(input->serverName, strlen(temp) + 1, "%s", temp);
    FREE(temp);
    if(input->serverName == NULL)
    {
        return csvrSystemFailure;
    }

    return csvrSuccess;
}

/***************************************************************************************************************
 * @brief Functions to read all the data from the client socket.
 * 
 * @param[in, out] output 
 * @return This function return following value : 
 *          csvrInvalidInput,
 *          csvrCannotListenSocket,
 *          csvrCannotAcceptSocket,
 *          csvrSuccess
 ***************************************************************************************************************/
static csvrErrCode_e  csvrClientReader(csvrRequest_t *output)
{
    if(output == NULL)
    {
        printf("[INFO] Invalid server object input\n");
        return csvrInvalidInput;
    }

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
    if(message == NULL)
    {
        printf("[INFO] Cannot allocate message memory\n");
        return csvrSystemFailure;
    }
    memset(message, 0, DEFAULT_MESSAGE_ALLOCATION);

    /* Set timeout */
    fd_set fdset;
    struct timeval tv;FD_ZERO(&fdset);
    FD_SET(output->clientfd, &fdset);
    tv.tv_sec = 10;             /* 10 second timeout */
    tv.tv_usec = 0;

    while(1)
    {
        if (select(output->clientfd + 1, NULL, &fdset, NULL, &tv) == 1)
        {
            int so_error;
            socklen_t len = sizeof so_error;

            getsockopt(output->clientfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error != 0)
            {
                printf("[ERROR] Cannot read socket ");GET_ERRNO();
                break;
            }
        }
        else
        {
            printf("[DEBUG] Client %s connection timeout: ",output->clientAddress);GET_ERRNO();
            break;
        }
        
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
            memcpy(output->header, message, lenMessage);

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
                    output->contentLength   = contentLength;
                    if(output->contentLength == 0)
                    {
                        break;
                    }
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

    /* If no content-length in POST request header, do this */
    if(output->contentLength == 0 && flagReadBody == true)
    {
        FREE(message);
        return csvrNoContentLength;
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
    memcpy(output->message, message, lenMessage);
    FREE(message);
    return csvrSuccess;
}

/**
 * @brief Functions to read all the incoming data from newly created client socket from the listening procedure.
 * 
 * @param[in] input 
 * @param[out] output 
 * @return This function return following value : 
 *          csvrInvalidInput,
 *          csvrCannotListenSocket,
 *          csvrCannotAcceptSocket,
 *          csvrSuccess
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
    }while(0);
    pthread_mutex_unlock(&lockRead);
    return ret;
}

/************************************************************************************************************
 * @brief Function to register URI to server.
 * 
 * @param[in] input Input path linked list.
 * @param[in] path The requested url path.
 * @param[in] type Request type defined in csvrRequestType_e enumeration.
 * @return This function will return NULL if no URI of requested path not registered. Otherwise, it will return
 *         current pointer of the path linked list.
 *************************************************************************************************************/
struct csvrPathUrl_t *csvrSearchUri(struct csvrPathUrl_t *input, char *path, csvrRequestType_e type)
{
    struct csvrPathUrl_t * current = NULL;
    struct csvrPathUrl_t * head = NULL;
    if(input && path)
    {
        head = input;
        current = input;
        while(current)
        {
            if((strlen(current->name) == strlen(path)) && (current->type == type))
            {
                if(!memcmp(current->name, path, strlen(path)))
                {
                    break;
                }
            }
            current = current->next;
        }
    }
    input = head;
    return current;
}

static void *csvrProcessUserProcedureThreads(void *arg)
{
    csvrThreadsData_t *data = (csvrThreadsData_t*)arg;
    if(data == NULL)
    {
        sem_post(&waitThread);
        printf("[INFO] Invalid data during user procedure\n");
        pthread_exit(NULL);
    }

    if(data->server == NULL || data->request == NULL)
    {
        if(data->server == NULL) printf("[INFO] Invalid data->server during user procedure\n");
        if(data->request == NULL) printf("[INFO] Invalid data->request during user procedure\n");
        csvrReadFinish(data->request, NULL);
        sem_post(&waitThread);
        pthread_exit(NULL);
    }

    sem_post(&waitThread);
    csvrIncreaseConnectionCounter();

    do
    {
        csvrErrCode_e readStatus = 0;
        readStatus = csvrClientReader(data->request);
        if(readStatus == csvrSuccess)
        {
            struct csvrPathUrl_t * path = NULL;
            struct csvrPathUrl_t * current = NULL;
            pthread_mutex_lock(&lockSearchPath);
            current = data->server->path;
            path = csvrSearchUri(current, data->request->path, data->request->type);
            pthread_mutex_unlock(&lockSearchPath);
            if(path)
            {
                printf("[%s %s] %s\n",typeStringTranslator[data->request->type], data->request->clientAddress, data->request->path);
                (*path->callbackFunction)(data->request, data->userData);
            }
            /* If path not found */
            else
            {
                csvrSendResponseError(data->request, csvrResponseNotFound, "Not Found");
                csvrReadFinish(data->request, NULL);
            }
        }
        else if(readStatus == csvrFailedReadSocket)
        {
            printf("\n[ERROR][ <<< ] Cannot read client socked: %s\n", strerror(errno));
            csvrReadFinish(data->request, NULL);
        }
        else if(readStatus == csvrNoContentLength)
        {
            csvrSendResponseError(data->request, csvrResponseLengthRequired, "Length Required");
            csvrReadFinish(data->request, NULL);
        }
        else
        {
            csvrSendResponseError(data->request, csvrResponseInternalServerError, "Internal Server Error");
            csvrReadFinish(data->request, NULL);
        }
        FREE(data->request);
    } while (0);

    csvrDecreaseConnectionCounter();

    pthread_exit(NULL);
}

void csvrAsyncronousThreadsCleanUp(void *arg)
{
    csvrThreadsData_t *data = (csvrThreadsData_t *)arg;
    if(data)
    {
        if(data->request)
        {
            csvrReadFinish(data->request, NULL);
            FREE(data->request);
        }
        FREE(data);
    }
}

static void *csvrAsyncronousThreads(void * arg)
{
    csvrThreadsData_t *threadsData = (csvrThreadsData_t *)arg;
    if(threadsData == NULL || threadsData->server == NULL) pthread_exit(NULL);

    threadsData->server->asyncFlag = true;

    pthread_cleanup_push(csvrAsyncronousThreadsCleanUp, (void*)threadsData);
    if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
    {
        pthread_exit(NULL);
    }

    sem_init(&waitThread,0,0);
    printf("[INFO] Server is listening at port %u\n", threadsData->server->port);

    pthread_mutex_t lockThreads = PTHREAD_MUTEX_INITIALIZER;
    while(1)
    {

        pthread_mutex_lock(&lockThreads);
        csvrRequest_t *request = NULL;
        request = calloc(1, sizeof(csvrRequest_t));
        if(request == NULL)
        {
            pthread_mutex_unlock(&lockThreads);
            break;
        }

        /* save the pointer here */
        memset(request,0,sizeof(csvrRequest_t));
        request->serverName  = strdup(threadsData->server->serverName);
        /* 1. Listen to socket */
        if (listen(threadsData->server->sockfd, totalConnectionAllowed) != 0)
        {
            printf("[INFO] Failed listen socket: %s\n", strerror(errno));
            pthread_mutex_unlock(&lockThreads);
            FREE(request);
            break;
        }

        struct sockaddr_in client;  
        memset(&client,0,sizeof(struct sockaddr_in));

        /* 2. Get the socket configuration to get the client address */
        socklen_t peerAddrSize = sizeof(client);
        request->clientfd = -1;
        request->clientfd = accept(threadsData->server->sockfd, (struct sockaddr*)&client, &peerAddrSize);
        if (request->clientfd < 0)
        {
            printf("[ERROR] Cannot accept client\n");
            csvrReadFinish(request, NULL);
            pthread_mutex_unlock(&lockThreads);
            FREE(request);
            break;
        }

        struct in_addr ipAddr = client.sin_addr;
        memset(request->clientAddress,0,sizeof(request->clientAddress));
        inet_ntop(AF_INET, &ipAddr, request->clientAddress, INET_ADDRSTRLEN );

        /* if success, do the procedure based on the path */
        pthread_t threadClient;
        threadsData->request = request;
        int status = pthread_create(&threadClient, NULL, csvrProcessUserProcedureThreads, (void*)threadsData);
        /* If success, detach the threads */
        if(status == 0)
        {
            pthread_mutex_unlock(&lockThreads);
            pthread_detach(threadClient);
        }
        /* If failed, send response and close the connection */
        else
        {
            pthread_mutex_unlock(&lockThreads);
            printf("[ERROR] Failed spawn client worker threads\n");
            if(csvrClientReader(request) == csvrSuccess)
            {
                //TODO : Check this section more. For now, we don't care about the content.
            }
            csvrSendResponseError(request, csvrResponseInternalServerError, "Internal Server Error");
            csvrReadFinish(request, NULL);
            FREE(request);
        }

        // TODO check this.
        /** why the threads still not able to get the threadsData pointer inside csvrProcessUserProcedureThreads threads
         *  if this sleep not exists.
         */
        usleep(200000);

    }
    threadsData->server->asyncFlag = false;
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

/************************************************************************************************************
 * @brief If this function is called, it will spawn threads to handle incoming request
 * 
 * @param[in] server 
 * @param[in] output 
 * @return csvrErrCode_e 
 *************************************************************************************************************/
csvrErrCode_e csvrServerStart(csvrServer_t *server, void *userData)
{
    if(server == NULL)
    {
        return csvrInvalidInput;
    }

    if(server->asyncFlag == true) return csvrNotAnError;

    csvrThreadsData_t *threadsData = NULL;
    threadsData = calloc(1, sizeof(csvrThreadsData_t));
    memset(threadsData, 0, sizeof(csvrThreadsData_t));
    threadsData->server = server;
    threadsData->userData = userData;

    int status = pthread_create(&serverThreads,NULL,csvrAsyncronousThreads,(void*)threadsData);
    if(status != 0)
    {
        FREE(threadsData);
        return csvrSystemFailure;
    }

    /* If server initialization procedure above success, init the signal */
    csvrInitSignal();

    return csvrSuccess;
}

/************************************************************************************************************
 * @brief Function to send response
 * 
 * @param[in] request Pointer to request structure, must not NULL.
 * @param[in] response Pointer to response structure, must not NULL.
 * @return csvrErrCode_e 
 *************************************************************************************************************/
csvrErrCode_e csvrSendResponse(csvrRequest_t * request, csvrResponse_t *response)
{
    if(request == NULL || response == NULL)
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
    
    char *payload = NULL;
    int retprint = -1;
    retprint = asprintf(&payload,
        "HTTP/1.1 200 OK\r\n"
        "Server: %s\r\n"
        "Accept-Ranges: none\r\n"
        "Vary: Accept-Encoding\r\n"
        "Last-Modified: %s\r\n"
        "Connection: closed\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %lu\r\n"
        "\r\n"
        "%s", 
        request->serverName,
        dtime, 
        strlen(response->body),
        response->body);

    if(retprint == -1)
    {
        return csvrSystemFailure;
    }

    ssize_t sendStatus = 0;
    sendStatus = send(request->clientfd, payload, strlen(payload), 0);

    if(sendStatus > 0)
    {
        ret = csvrSuccess;
    }
    else
    {
        ret = csvrFailedSendData;
    }

    FREE(payload);
    return ret;
}

/************************************************************************************************************
 * @brief Function to send HTTP error
 * 
 * @param[in] request 
 * @param[in] code 
 * @param[in] desc 
 * @return csvrErrCode_e 
 *************************************************************************************************************/
csvrErrCode_e csvrSendResponseError(csvrRequest_t * request, csvrHttpResponseCode_e code, char*desc)
{
    if(request == NULL || desc == NULL)
    {
        return csvrInvalidInput;
    }

    csvrErrCode_e ret = csvrSuccess;
    char dtime[100];
    memset(dtime,0,sizeof(dtime));
    time_t now = time(0);
    struct tm *tm = gmtime(&now);
    strftime(dtime, sizeof(dtime), "%a, %d %b %Y %H:%M:%S %Z", tm);
    
    const char *template = "HTTP/1.1 %d %s\r\n"
                            "Server: %s\r\n"
                            "Accept-Ranges: none\r\n"
                            "Vary: Accept-Encoding\r\n"
                            "Last-Modified: %s\r\n"
                            "Connection: closed\r\n"
                            "%s"
                            "\r\n"
                            "%s"
                            ;
    char *message = NULL;
    char *payload = NULL;
    char *header  = NULL;
    int retprint  = -1;
    if(createHttpErrorResponse(&message, request, code) == csvrSuccess)
    {
        retprint = asprintf(&header,
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %lu\r\n",
            strlen(message));
        if(retprint == -1)
        {
            /* Send empty reply */
            printf("[%s %s] %s Empty reply - Failed allocate memory\n",typeStringTranslator[request->type], request->clientAddress, request->path);
            send(request->clientfd, "", 0, 0);
            FREE(payload);
            FREE(message);
            return csvrSuccess;
        }
    }
    
    retprint = asprintf(&payload, template, 
        code, desc,
        request->serverName,
        dtime,
        header ? header : "",
        message ? message : ""
        );

    ssize_t sendStatus = 0;
    if(retprint == -1)
    {
        /* Send empty reply */
        printf("[%s %s] %s Empty reply - Failed allocate memory\n",typeStringTranslator[request->type], request->clientAddress, request->path);
        send(request->clientfd, "", 0, 0);
    }
    else
    {
        printf("[%s %s] %s %i %s\n",typeStringTranslator[request->type], request->clientAddress, request->path, code, desc);
        sendStatus = send(request->clientfd, payload, strlen(payload), 0);
    }

    if(sendStatus > 0)
    {
        ret = csvrSuccess;
    }
    else
    {
        ret = csvrFailedSendData;
    }

    FREE(payload);
    FREE(message);
    return ret;
}

/************************************************************************************************************
 * @brief Function to close the client socket and releasing all the request and/or response resourced that 
 *        has been used.
 * 
 * @param[in,out] request  NULL allowed
 * @param[in,out] response NULL allowed
 * @return This function always return csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrReadFinish(csvrRequest_t *request, csvrResponse_t *response)
{
    if(request)
    {
        int sockType = 0;
        int ret = 0;
        socklen_t optlen = sizeof(sockType);
        ret = getsockopt(request->clientfd,SOL_SOCKET,SO_TYPE, &sockType, &optlen);
        /* Shutdown the read and write connections */
        if(ret == 0)
        {
            ret = shutdown(request->clientfd, SHUT_RDWR);
            /* catch the errno */
            if(ret == 0)
            {
                ret = close(request->clientfd);
                if(ret < 0)
                {
                    printf("[ERROR] Closing client socket failed: (%d) %s",errno, strerror(errno));
                }
            }
        }
        FREE(request->message);
        FREE(request->header);
        FREE(request->content);
        FREE(request->serverName);
    }

    if(response)
    {
        /* Free header if any */
        int i = 0;
        for(;i<response->header.total;i++)
        {
            FREE(response->header.data[i]);
        }
        FREE(response->header.data);

        /* Free body */
        FREE(response->body);
    }

    return csvrSuccess;
}

/************************************************************************************************************
 * @brief Function to close the main socket that has been opened by csvrInit,
 *        releasing all the allocated memory in the csvrServer_t pointer variable, and
 *        releasing the csvrPathUrl_t linked list object pointer.
 * 
 * @param[in,out] server 
 * @return This function always return csvrSuccess 
 *************************************************************************************************************/
csvrErrCode_e csvrShutdown(csvrServer_t *server)
{
    if(server == NULL)
    {
        return csvrInvalidInput;
    }

    printf("[INFO] Shutdown server\n");
    if(server->asyncFlag)
    {
        printf("[INFO] Canceling threads\n");
        pthread_cancel(serverThreads);
        pthread_join(serverThreads, NULL);
    }

    FREE(server->serverName);

    printf("[INFO] Closing running socket\n");
    int sockType = 0;
    int ret = 0;
    socklen_t optlen = sizeof(sockType);
    ret = getsockopt(server->sockfd,SOL_SOCKET,SO_TYPE,&sockType, &optlen);
    /* Shutdown the read and write connections */
    if(ret == 0)
    {
        ret = shutdown(server->sockfd, SHUT_RDWR);
        /* catch the errno */
        if(ret == 0)
        {
            ret = close(server->sockfd);
            if(ret < 0)
            {
                printf("[INFO] Closing server socket failed: (%d) %s",errno, strerror(errno));
            }
            else
            {
                printf("[INFO] Server closed properly\n");
            }
        }
    }

    if(server->path)
    {
        struct csvrPathUrl_t* current = server->path;
        struct csvrPathUrl_t* next = NULL;
        while (current)
        {
            next = current->next;
            printf("[INFO] Cleaning URI %s\n",current->name);
            FREE(current->name);
            FREE(current);
            current = next;
        }

        printf("[INFO] Cleaning URI finished\n");
    }

    free(server);
    printf("[INFO] Shutdown procedure finished\n");

    csvrDestroySignal();

    return csvrSuccess;
}

/************************************************************************************************************
 * @brief This function will wait for any termination signal which has been initialized by csvrInitSignal.
 * 
 * @param server 
 * @return This function return following values:
 *          csvrSuccess,
 *          csvrNotAnError
 *************************************************************************************************************/
csvrErrCode_e csvrJoin(csvrServer_t *server)
{
    if(!csvrWaitSignal())
    {
        return csvrSuccess;
    }
    return csvrNotAnError;
}

/************************************************************************************************************
 * @brief Function to get current total client connection socket which is saved in the totalConnectionNow variable.
 *        The value will be reset if the application is restarted.
 * 
 * @return This function will return the total of current socket client connections.
 *************************************************************************************************************/
long csvrGetTotalConnection()
{
    pthread_mutex_lock(&lockCount);
    long total = totalConnectionNow;
    pthread_mutex_unlock(&lockCount);
    return total;
}

/************************************************************************************************************
 * @brief Function to create custom header response.
 * 
 * @param[in,out] input pointer to csvrResponse_t variable.
 * @param[in] key the header key. Example : "Accept",  "Content-Type", "your-own-header",etc.
 * @param[in] value the value of the header key. Example : "application/json", "text/html", "Your own header key" etc.
 * 
 * @return This function will return following value : 
 *      csvrInvalidInput
 *      csvrNotAnError
 *      csvrSystemFailure
 *      csvrSuccess
 *************************************************************************************************************/
csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value)
{
    if(input == NULL || key == NULL || value == NULL)
    {
        return csvrInvalidInput;
    }

    char *buffer = NULL;
    int retprint = asprintf(&buffer,"%s: %s%s",key,value,HEADER_SEPARATOR);
    if(retprint == -1)
    {
        return csvrSystemFailure;
    }

    char **newHeader = NULL;
    newHeader = calloc(input->header.total + 1, sizeof(char*));
    if(newHeader == NULL)
    {
        FREE(buffer);
        return csvrSystemFailure;
    }
    input->header.total++;

    size_t i = 0;
    for(;i < (input->header.total - 1);i++)
    {
        retprint = asprintf(&(newHeader[i]), "%s", input->header.data[i]);
        if(retprint == -1)
        {
            FREE(buffer);
            return csvrSystemFailure;
        }
        FREE(input->header.data[i]);
    }
    retprint = asprintf(&(newHeader[input->header.total - 1]), "%s", buffer);
    if(retprint == -1)
    {
        FREE(buffer);
        return csvrSystemFailure;
    }
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
csvrErrCode_e csvrAddPath(csvrServer_t *input, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrRequest_t *, void *))
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
        return csvrSystemFailure;
    }

    FREE(input->body);
    ret = asprintf(&(input->body), "%s", bodyTemp);
    if(ret == -1)
    {
        FREE(bodyTemp);
        return csvrSystemFailure;
    }
    FREE(bodyTemp);
    return csvrSuccess;
}

#ifdef TEST
void *handlerRequest(csvrRequest_t *request, void *userData)
{
    csvrResponse_t response;
    memset(&response,0, sizeof(response));
    printf("%s -> OK\n",request->path);
    csvrReadFinish(request, &response);

    return NULL;
}


/**
 * @brief Compile with :
 * 
 *    $ gcc src/*.c -Iinclude/ -o test -D_GNU_SOURCE -DTEST -lpthread
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
    memset(&response, 0, sizeof(csvrResponse_t));
    csvrAddCustomHeader(&response, "session-uuid","a58ae080-6efc-11ed-b9d7-0800274047c4");
    csvrAddCustomHeader(&response, "sender-name","Ergi");
    csvrAddCustomHeader(&response, "sender-address","Jakarta");
    int i = 0;
    for(;i < response.header.total;i++)
    {
        printf("[%d] %s\n",i,response.header.data[i]);
    }
    csvrReadFinish(NULL,&response);

    csvrServer_t * server  = NULL;
    server = calloc(1, sizeof(csvrServer_t));
    memset(server, 0, sizeof(csvrServer_t));

    csvrAddPath(server, "/", csvrTypePost, handlerRequest);
    csvrAddPath(server, "/time", csvrTypeGet, handlerRequest);
    csvrAddPath(server, "/roamer", csvrTypePost, handlerRequest);
    
    struct csvrPathUrl_t * link = NULL;
    
    csvrRequest_t *request = NULL;
    request = calloc(1, sizeof(csvrRequest_t));
    memset(request, 0, sizeof(csvrRequest_t));
    strcpy(request->path,"/time");
    link = csvrSearchUri(server->path, "/time", csvrTypeGet);
    (*link->callbackFunction)(request, &response);
    
    memset(request, 0, sizeof(csvrRequest_t));
    strcpy(request->path,"/roamer");
    link = csvrSearchUri(server->path, "/roamer", csvrTypePost);
    (*link->callbackFunction)(request, &response);

    memset(request, 0, sizeof(csvrRequest_t));
    strcpy(request->path,"/");
    link = csvrSearchUri(server->path, "/", csvrTypePost);
    (*link->callbackFunction)(request, &response);
    FREE(request);
    csvrShutdown(server);
    return 0;
}
#endif