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

/* don't care about the case. set all to lower cases */
#define HEADER_CONTENT_LENGTH_KEY "content-length: "
#define HEADER_CONTENT_TYPE_KEY   "content-type: "
#define DEFAULT_MESSAGE_ALLOCATION 3
#define DEFAULT_SERVER_NAME       CSVR_NAME"-"CSVR_VERSION

#define MINIMUM_REQUEST_TYPE_LENGTH strlen("PUT")
#define MAXIMUM_REQUEST_TYPE_LENGTH strlen("DELETE")

static long _totalConnectionNow = 0;
static int _totalConnectionAllowed = 100;
static pthread_t _serverThreads;
static sem_t _waitThread;
static pthread_mutex_t _lockRead = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _lockCount = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _lockSearchPath = PTHREAD_MUTEX_INITIALIZER;

/***************************************************************************************************************
 * @brief Structure to save all the allocated pointer information during 
 *        csvrAsyncronousThreads and csvrProcessUserProcedureThreads. The pointer
 *        must be freed in the threads callback function.
 ***************************************************************************************************************/
typedef struct
{
    csvrServer_t *server;
    csvrRequest_t *request;
}csvrThreadsData_t;

/***************************************************************************************************************
 * @brief: For logging purpose
 ***************************************************************************************************************/
#if 0 //curently this variable is not used
static size_t _lengthTypeTranslator[csvrTypeMax] = 
{
    [csvrTypeNotKnown ] = 0,
    [csvrTypeGet      ] = 3,
    [csvrTypePut      ] = 3,
    [csvrTypePost     ] = 4,
    [csvrTypeHead     ] = 4,
    [csvrTypeDelete   ] = 6,
    [csvrTypeUnknwon  ] = 0,
};
#endif

/***************************************************************************************************************
 * @brief: For logging purpose
 ***************************************************************************************************************/
static char *_typeStringTranslator[] = 
{
    [csvrTypeNotKnown ] = "",
    [csvrTypeGet      ] = "GET ",
    [csvrTypePut      ] = "PUT ",
    [csvrTypePost     ] = "POST",
    [csvrTypeHead     ] = "HEAD",
    [csvrTypeDelete   ] = "DELETE",
    [csvrTypeUnknwon  ] = "",
};

CSVR_STATIC void csvrIncreaseConnectionCounter()
{
    pthread_mutex_lock(&_lockCount);
    _totalConnectionNow++;
    pthread_mutex_unlock(&_lockCount);
}

CSVR_STATIC void csvrDecreaseConnectionCounter()
{
    pthread_mutex_lock(&_lockCount);
    _totalConnectionNow--;
    pthread_mutex_unlock(&_lockCount);
}

csvrRequestType_e getRequestType(char*header)
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

static void csvrConvertLowerCase(char*data, size_t length)
{
    size_t index = 0;
    while(index < length)
    {
        char temp = tolower(data[index]);
        data[index] = temp;
        index++;
    }
}

int csvrGetContentLength(char*header, size_t headerLen)
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
        if(!memcmp(header+index, CSVR_HEADER_SEPARATOR, strlen(CSVR_HEADER_SEPARATOR)))
        {
            size_t lengthLine = index - lastIndex;
            line = calloc((lengthLine + 1), sizeof(char));
            if(line)
            {
                memset(line, 0, (lengthLine + 1));
                memcpy(line, header + lastIndex, lengthLine);
                csvrConvertLowerCase(line, lengthLine);
                if(!memcmp(line,HEADER_CONTENT_LENGTH_KEY, strlen(HEADER_CONTENT_LENGTH_KEY)))
                {
                    memset(buffer, 0, sizeof(buffer));
                    snprintf(buffer,sizeof(buffer), "%s", line + strlen(HEADER_CONTENT_LENGTH_KEY));
                    contentLength = atoi(buffer);
                    break;
                }
                CSVR_FREE(line);
            }
            lastIndex = index + strlen(CSVR_HEADER_SEPARATOR);
        }
        index++;
    }
    CSVR_FREE(line);
    return contentLength;
}

int csvrGetHeaderFromPayload(char **output, char*payload, size_t payloadLen)
{
    if(output == NULL || payload == NULL || payloadLen == 0)
    {
        return 0;
    }

    char *header = NULL;
    int index = 0;
    int headerLength = 0;

    while(index < payloadLen)
    {
        /* Get 0x0D 0x0A 0x0D 0x0A */
        if(!memcmp(payload+index, CSVR_BODY_SEPARATOR, strlen(CSVR_BODY_SEPARATOR)))
        {
            headerLength = index + strlen(CSVR_BODY_SEPARATOR);
            header = calloc((headerLength + 1), sizeof(char));
            if(header)
            {
                memset(header, 0, (headerLength + 1));
                memcpy(header, payload, headerLength);
            }
            break;
        }
        index++;
    }
    *output = header;
    return headerLength;
}

CSVR_STATIC char *contentTypeTranslator(csvrContentType_e contentType)
{
    switch(contentType){
        case applicationJson: return "application/json";
        case applicationJs: return "application/js";
        case textHtml: return "text/html";
        case textPlain: return "text/plain";
        default: break;
    }
    return "text/plain";
}

csvrContentType_e csvrGetContentType(char*header)
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
        if(!memcmp(header+index, CSVR_HEADER_SEPARATOR, strlen(CSVR_HEADER_SEPARATOR)))
        {
            size_t lengthLine = index - lastIndex;
            line = calloc((lengthLine + 1), sizeof(char));
            if(line)
            {
                memset(line, 0, (lengthLine + 1));
                memcpy(line, header + lastIndex, lengthLine);
                csvrConvertLowerCase(line, lengthLine);
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
                    else if(!memcmp(buffer, "application/javascript", strlen("application/javascript")))
                    {
                        contentType = applicationJson;
                    }
                    else if(!memcmp(buffer, "text/html", strlen("text/html")))
                    {
                        contentType = textHtml;
                    }
                    else if(!memcmp(buffer, "text/plain", strlen("text/plain")))
                    {
                        contentType = textPlain;
                    }
                    break;
                }
                CSVR_FREE(line);
            }
            lastIndex = index + strlen(CSVR_HEADER_SEPARATOR);
        }
        index++;
    }
    CSVR_FREE(line);

    return contentType;
}

CSVR_STATIC csvrErrCode_e getHeaderKeyValue(char*header, char*key, char *dest, size_t maxlen)
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
        if(!memcmp(header+index, CSVR_HEADER_SEPARATOR, strlen(CSVR_HEADER_SEPARATOR)))
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
                CSVR_FREE(line);
            }
            lastIndex = index + strlen(CSVR_HEADER_SEPARATOR);
        }
        index++;
    }
    CSVR_FREE(line);

    return ret;
}

CSVR_STATIC csvrErrCode_e getRequestUriPath(char*header, char*dest, size_t maxlen)
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

CSVR_STATIC csvrHttpVersion_e getHttpVersion(char*header)
{
    csvrHttpVersion_e version = http1_0;
    size_t len = 0;
    char buffer[10];

    char *ptr1 = NULL;
    char *ptr2 = NULL;
    ptr1 = strstr(header, "HTTP/");
    if(ptr1)
    {
        ptr2 = strstr(ptr1, CSVR_HEADER_SEPARATOR);
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
        CSVR_FREE(res);
    }

    return version;
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
CSVR_STATIC csvrErrCode_e  csvrClientReader(csvrRequest_t *output)
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
                printf("[ERROR] Cannot read socket ");CSVR_PRINT_ERRNO();
                break;
            }
        }
        else
        {
            printf("[DEBUG] Client %s connection timeout: ",output->clientAddress);CSVR_PRINT_ERRNO();
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
        if(!memcmp(message + lenMessage - strlen(CSVR_BODY_SEPARATOR), CSVR_BODY_SEPARATOR, strlen(CSVR_BODY_SEPARATOR)))
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
                    output->contentType     = csvrGetContentType(output->header);
                    contentLength           = csvrGetContentLength(output->header, lenMessage);
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
        CSVR_FREE(message);
        return csvrNoContentLength;
    }

    if(retval <= 0)
    {
        printf("Read failed : %s\n",strerror(errno));
        close(output->clientfd);
        CSVR_FREE(message);
        CSVR_FREE(output->header);
        CSVR_FREE(output->content);
        return csvrFailedReadSocket;
    }

    output->message = calloc(lenMessage + 1, sizeof(char));
    memset(output->message, 0, lenMessage + 1);
    memcpy(output->message, message, lenMessage);
    CSVR_FREE(message);
    return csvrSuccess;
}

/************************************************************************************************************
 * @brief Function to search for registered URI in the csvrPathUrl_t object.
 * 
 * @param[in] input Pointer to the csvrPathUrl_t linked list.
 * @param[in] path The requested URI path.
 * @param[in] type The request type defined in csvrRequestType_e enumeration.
 * @return This function will return NULL if no URI of requested path not registered. Otherwise, it will return
 *         current pointer of the path linked list.
 *************************************************************************************************************/
CSVR_STATIC struct csvrPathUrl_t *csvrSearchUri(struct csvrPathUrl_t *input, char *path, csvrRequestType_e type)
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

CSVR_STATIC void *csvrProcessUserProcedureThreads(void *arg)
{
    csvrThreadsData_t *data = (csvrThreadsData_t*)arg;
    if(data == NULL)
    {
        sem_post(&_waitThread);
        printf("[INFO] Invalid data during user procedure\n");
        pthread_exit(NULL);
    }

    if(data->server == NULL || data->request == NULL)
    {
        if(data->server == NULL) printf("[INFO] Invalid data->server during user procedure\n");
        if(data->request == NULL)
        {
            printf("[INFO] Invalid data->request during user procedure\n");
        }
        else
        {
            csvrReadFinish(data->request, NULL);
            CSVR_FREE(data->request);
            CSVR_FREE(data);
        }
        sem_post(&_waitThread);
        pthread_exit(NULL);
    }

    sem_post(&_waitThread);
    csvrIncreaseConnectionCounter();

    do
    {
        csvrErrCode_e readStatus = 0;
        readStatus = csvrClientReader(data->request);
        if(readStatus == csvrSuccess)
        {
            struct csvrPathUrl_t * path = NULL;
            struct csvrPathUrl_t * current = NULL;
            pthread_mutex_lock(&_lockSearchPath);
            current = data->server->path;
            path = csvrSearchUri(current, data->request->path, data->request->type);
            pthread_mutex_unlock(&_lockSearchPath);
            if(path)
            {
                printf("[%s %s] %s\n",_typeStringTranslator[data->request->type], data->request->clientAddress, data->request->path);
                (*path->callbackFunction)(data->request, path->userData);
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
        CSVR_FREE(data->request);
        CSVR_FREE(data);
    } while (0);

    csvrDecreaseConnectionCounter();

    pthread_exit(NULL);
}

CSVR_STATIC void csvrAsyncronousThreadsCleanUp(void *arg)
{
    if(arg)
    {
        if(((csvrThreadsData_t *)arg)->request)
        {
            csvrReadFinish(((csvrThreadsData_t *)arg)->request, NULL);
            CSVR_FREE(((csvrThreadsData_t *)arg)->request);
        }
        CSVR_FREE(arg);
    }
}

CSVR_STATIC void *csvrAsyncronousThreads(void * arg)
{
    csvrThreadsData_t *threadsData = (csvrThreadsData_t *)arg;
    if(threadsData == NULL || threadsData->server == NULL) pthread_exit(NULL);

    threadsData->server->asyncFlag = true;

    pthread_cleanup_push(csvrAsyncronousThreadsCleanUp, (void*)threadsData);
    if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
    {
        pthread_exit(NULL);
    }

    sem_init(&_waitThread,0,0);
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
        threadsData->request = request;

        memset(request,0,sizeof(csvrRequest_t));
        memcpy(request->serverName, threadsData->server->serverName, sizeof(request->serverName) - 1);

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
            CSVR_FREE(request);
            break;
        }

        struct in_addr ipAddr = client.sin_addr;
        memset(request->clientAddress,0,sizeof(request->clientAddress));
        inet_ntop(AF_INET, &ipAddr, request->clientAddress, INET_ADDRSTRLEN );

        /* if success, do the procedure based on the path */
        pthread_t threadClient;
        csvrThreadsData_t *userThreadsData = NULL;
        userThreadsData = calloc(1, sizeof(csvrThreadsData_t));
        if(userThreadsData == NULL)
        {
            CSVR_FREE(request);
            pthread_mutex_unlock(&lockThreads);
            break;
        }

        /* save the pointer again here */
        userThreadsData->request  = request;
        userThreadsData->server   = threadsData->server;
        int status = pthread_create(&threadClient, NULL, csvrProcessUserProcedureThreads, (void*)userThreadsData);
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
            CSVR_FREE(request);
        }

        // TODO check this.
        /** why the threads still not able to get the threadsData pointer inside csvrProcessUserProcedureThreads threads
         *  if this sleep not exists.
         */
        USLEEP(200000);

    }
    threadsData->server->asyncFlag = false;
    pthread_cleanup_pop(0);
    pthread_exit(NULL);
}

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
        printf("[ERROR] Cannot create socket: ");CSVR_PRINT_ERRNO();
        free(server);
        return NULL;
    }

    int enable = 1;
    if(setsockopt(server->sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
    {
        printf("[ERROR] Cannot setsockopt socket: ");CSVR_PRINT_ERRNO();
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
            printf("[ERROR] Cannot create socket: ");CSVR_PRINT_ERRNO();
            close(server->sockfd);
            free(server);
            return NULL;
        }
        printf("[DEBUG] Try %d binding socket failed: ", tryTimes);CSVR_PRINT_ERRNO();
        tryTimes++;
        /* Delay 1 second */
        USLEEP(1000000);
    }

    /**< 4. Listen to the newly bindd socket *///
    if (listen(server->sockfd, _totalConnectionAllowed) != 0)
    {
        printf("[ERROR] Failed listen socket: %s\n", strerror(errno));
        close(server->sockfd);
        free(server);
        return NULL;
    }

    memset(server->serverName, 0, sizeof(server->serverName));
    snprintf(server->serverName, sizeof(server->serverName), "%s-%s", (char*)CSVR_NAME, (char*)CSVR_VERSION);

    server->path = NULL;
    return server;
}

csvrErrCode_e csvrSetCustomServerName(csvrServer_t *server, char *serverName, ...)
{
    if(server == NULL || serverName == NULL)
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
        return csvrSystemFailure;
    }

    if(strlen(temp) == 0)
    {
        CSVR_FREE(temp);
        return csvrNotAnError;
    }

    memset(server->serverName, 0, strlen(temp) + 1);
    memcpy(server->serverName, temp, sizeof(server->serverName));
    CSVR_FREE(temp);
    return csvrSuccess;
}

csvrErrCode_e csvrServerStart(csvrServer_t *server)
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

    int status = pthread_create(&_serverThreads,NULL,csvrAsyncronousThreads,(void*)threadsData);
    if(status != 0)
    {
        CSVR_FREE(threadsData);
        return csvrSystemFailure;
    }

    /* If server initialization procedure above success, init the signal */
    csvrSignalInit();

    return csvrSuccess;
}

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
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "\r\n"
        "%s", 
        request->serverName,
        dtime,
        (response->contentType) ? contentTypeTranslator(response->contentType) : "text/plain",
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

    CSVR_FREE(payload);
    return ret;
}

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
    if(csvrResponseGenerateHTMLContent(&message, request, code) == csvrSuccess)
    {
        retprint = asprintf(&header,
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %lu\r\n",
            strlen(message));
        if(retprint == -1)
        {
            /* Send empty reply */
            printf("[%s %s] %s Empty reply - Failed allocate memory\n",_typeStringTranslator[request->type], request->clientAddress, request->path);
            send(request->clientfd, "", 0, 0);
            CSVR_FREE(payload);
            CSVR_FREE(message);
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
        printf("[%s %s] %s Empty reply - Failed allocate memory\n",_typeStringTranslator[request->type], request->clientAddress, request->path);
        send(request->clientfd, "", 0, 0);
    }
    else
    {
        printf("[%s %s] %s %i %s\n",_typeStringTranslator[request->type], request->clientAddress, request->path, code, desc);
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

    CSVR_FREE(header);
    CSVR_FREE(payload);
    CSVR_FREE(message);
    return ret;
}

csvrErrCode_e csvrRead(csvrServer_t *server, csvrRequest_t *output)
{
    if(server == NULL || output == NULL)
    {
        return csvrInvalidInput;
    }

    csvrErrCode_e ret = csvrSystemFailure;
    pthread_mutex_lock(&_lockRead);
    do
    {
        struct sockaddr_in client;  
        memset(&client,0,sizeof(struct sockaddr_in));

        /* 2. Get the socket configuration to get the client address */
        socklen_t peerAddrSize = sizeof(client);
        output->clientfd = -1;
        output->clientfd = accept(server->sockfd, (struct sockaddr*)&client, &peerAddrSize);
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
    pthread_mutex_unlock(&_lockRead);
    return ret;
}

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
        CSVR_FREE(request->message);
        CSVR_FREE(request->header);
        CSVR_FREE(request->content);
    }

    if(response)
    {
        /* Free header if any */
        int i = 0;
        for(;i<response->header.total;i++)
        {
            CSVR_FREE(response->header.data[i]);
        }
        CSVR_FREE(response->header.data);

        /* Free body */
        CSVR_FREE(response->body);
    }

    return csvrSuccess;
}

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
        pthread_cancel(_serverThreads);
        pthread_join(_serverThreads, NULL);
    }

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
            CSVR_FREE(current->name);
            CSVR_FREE(current);
            current = next;
        }

        printf("[INFO] Cleaning URI finished\n");
    }

    free(server);
    printf("[INFO] Shutdown procedure finished\n");

    csvrSignalDestroy();

    return csvrSuccess;
}

csvrErrCode_e csvrJoin(csvrServer_t *server)
{
    if(!csvrSignalWait())
    {
        return csvrSuccess;
    }
    return csvrNotAnError;
}

long csvrGetTotalConnection()
{
    pthread_mutex_lock(&_lockCount);
    long total = _totalConnectionNow;
    pthread_mutex_unlock(&_lockCount);
    return total;
}

csvrErrCode_e csvrAddCustomHeader(csvrResponse_t*input, char *key, char*value)
{
    if(input == NULL || key == NULL || value == NULL)
    {
        return csvrInvalidInput;
    }

    char *buffer = NULL;
    int retprint = asprintf(&buffer,"%s: %s%s",key,value,CSVR_HEADER_SEPARATOR);
    if(retprint == -1)
    {
        return csvrSystemFailure;
    }

    char **newHeader = NULL;
    newHeader = calloc(input->header.total + 1, sizeof(char*));
    if(newHeader == NULL)
    {
        CSVR_FREE(buffer);
        return csvrSystemFailure;
    }
    input->header.total++;

    size_t i = 0;
    for(;i < (input->header.total - 1);i++)
    {
        retprint = asprintf(&(newHeader[i]), "%s", input->header.data[i]);
        if(retprint == -1)
        {
            CSVR_FREE(buffer);
            return csvrSystemFailure;
        }
        CSVR_FREE(input->header.data[i]);
    }
    retprint = asprintf(&(newHeader[input->header.total - 1]), "%s", buffer);
    if(retprint == -1)
    {
        CSVR_FREE(buffer);
        return csvrSystemFailure;
    }
    CSVR_FREE(buffer);

    CSVR_FREE(input->header.data)
    input->header.data = newHeader;
    return csvrSuccess;
}

csvrErrCode_e csvrAddPath(csvrServer_t *server, char *path, csvrRequestType_e type, void *(*callbackFunction)(csvrRequest_t *, void *), void*userData)
{
    if(server == NULL || path == NULL || (*callbackFunction) == NULL)
    {
        return csvrInvalidInput;
    }

    /* Handle if server not initialize yet */
    if(server->sockfd == -1)
    {
        return csvrNotAnError;
    }

    /* Search if URI already exists */
    struct csvrPathUrl_t * current = NULL;
    if(server->path)
    {
        current = server->path;
        while(current)
        {
            /* Check if the path and the type is exists */
            if(!memcmp(current->name, path, strlen(path)) && current->type == type)
            {
                return csvrUriAlreadyExists;
            }
            current = current->next;
        }
    }

    /* Initialize new memory to saved the new path */
    struct csvrPathUrl_t * newPath = NULL;
    newPath = (struct csvrPathUrl_t*)malloc(sizeof(struct csvrPathUrl_t));
    if(newPath == NULL)
    {
        return csvrSystemFailure;
    }

    newPath->type = type;
    newPath->name = strdup(path);
    newPath->callbackFunction = (*callbackFunction);
    newPath->userData = userData;
    newPath->next = server->path;
    server->path = newPath;

    printf("[DEBUG] Initializing path '%s' with '%s' type success\n", newPath->name, _typeStringTranslator[newPath->type]);
    return csvrSuccess;
}

csvrErrCode_e csvrAddContent(csvrResponse_t *input, csvrContentType_e contentType, char *content, ...)
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

    CSVR_FREE(input->body);
    ret = asprintf(&(input->body), "%s", bodyTemp);
    if(ret == -1)
    {
        CSVR_FREE(bodyTemp);
        return csvrSystemFailure;
    }

    /* Set the content-type enumeration here */
    input->contentType = contentType;

    CSVR_FREE(bodyTemp);
    return csvrSuccess;
}

csvrErrCode_e csvrAddContentFromFile(csvrResponse_t *input, csvrContentType_e contentType, char *arg, ...)
{
    if(input == NULL || arg == NULL)
    {
        return csvrInvalidInput;
    }

    char * path = NULL;
    va_list aptr;
    va_start(aptr, arg);
    int ret = vasprintf(&path, arg, aptr);
    va_end(aptr);
    if(ret == -1)
    {
        return csvrSystemFailure;
    }

    FILE* fp = NULL;
    fp = fopen(path, "r");
    if(fp == NULL)
    {
        return csvrSystemFailure;
    }

    int c = 0;
    CSVR_FREE(input->body);

    char * temp = NULL;
    size_t length = 0;
    temp = calloc(2, sizeof(char));
    while((c = getc(fp)) != EOF)
    {
        temp[length] = c;
        temp = realloc(temp, (length + 2) * sizeof(char));
        length++;
    }

    input->body = temp;
    input->contentLength = length;
    input->contentType = contentType;

    fclose(fp);
    fp = NULL;

    CSVR_FREE(path);

    return csvrSuccess;
}