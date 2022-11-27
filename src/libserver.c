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

#include "libserver.h"

#define HEADER_CONTENT_LENGTH_KEY "Content-Length: "
#define HEADER_CONTENT_TYPE_KEY   "Content-Type: "
#define DEFAULT_MESSAGE_ALLOCATION 3

#define MINIMUM_REQUEST_TYPE_LENGTH strlen("PUT")
#define MAXIMUM_REQUEST_TYPE_LENGTH strlen("DELETE")

#ifndef FREE
#define FREE(ptr)	if(ptr != NULL)\
{\
  free(ptr);\
  ptr = NULL;\
}
#endif

static int serverSession = 0;
static int totalConnectionAllowed = 5;
pthread_mutex_t lockRead = PTHREAD_MUTEX_INITIALIZER;

size_t lengthTypeTranslator[maxType] = 
{
    [notKnown ] = 0,
    [get      ] = 3,
    [put      ] = 3,
    [post     ] = 4,
    [head     ] = 4,
    [delete] = 6,
};

char *typeStringTranslator[] = 
{
    [notKnown ] = "",
    [get      ] = "GET ",
    [put      ] = "PUT ",
    [post     ] = "POST ",
    [head     ] = "HEAD ",
    [delete   ] = "DELETE ",
};

static requestType_e getRequestType(char*header)
{
    requestType_e type = notKnown;
    if(!memcmp("POST", header, 4))
    {
        type = post;
    }
    else if(!memcmp("GET", header, 3))
    {
        type = get;
    }
    else if(!memcmp("HEAD", header, 4))
    {
        type = head;
    }
    else if(!memcmp("DELETE", header, 6))
    {
        type = delete;
    }
    else if(!memcmp("PUT", header, 3))
    {
        type = put;
    }
    return type;
}

size_t getContentLength(char*header)
{
    size_t contentLength = 0;
    size_t len = 0;
    char buffer[10];

    char *ptr1 = NULL;
    char *ptr2 = NULL;
    ptr1 = strstr(header, HEADER_CONTENT_LENGTH_KEY);
    if(ptr1)
    {
        ptr2 = strstr(ptr1, "\x0D\x0A");
        len = ptr2 - ptr1;
        char *res = (char*)malloc(sizeof(char)*(len+1));
        if(res)
        {
            strncpy(res, ptr1, len);
            res[len] = '\0';
            printf("%s\n",res);
            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, res + strlen(HEADER_CONTENT_LENGTH_KEY), sizeof(buffer));
            contentLength = atoi(buffer);
            free(res);
        }
    }

    return contentLength;
}

static contentType_e getContentType(char*header)
{
    contentType_e contentType = textHtml;
    size_t len = 0;
    char buffer[100];

    char *ptr1 = NULL;
    char *ptr2 = NULL;
    ptr1 = strstr(header, HEADER_CONTENT_TYPE_KEY);
    if(ptr1)
    {
        ptr2 = strstr(ptr1, "\x0D\x0A");
        len = ptr2 - ptr1;
        char *res = (char*)malloc(sizeof(char)*(len+1));
        if(res)
        {
            strncpy(res, ptr1, len);
            res[len] = '\0';
            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, res + strlen(HEADER_CONTENT_TYPE_KEY), strlen(res) - strlen(HEADER_CONTENT_TYPE_KEY));
            int i = 0;
            for(;i < strlen(buffer);i++)
            {
                char temp = 0;
                temp = tolower(buffer[i]);
                buffer[i] = temp;
            }

            if(!memcmp(buffer, "application/json", strlen("application/json")))
            {
                contentType = applicationJson;
            }
            else 
            {
                
            }
            free(res);
        }
    }

    return contentType;
}

static size_t getHeaderKeyValue(char*header, char*key, char *dest, size_t maxlen)
{

    size_t len = 0;
    size_t ret = 0;
    char keyBuffer[100];
    char buffer[10];
    memset(buffer, 0, sizeof(buffer));
    memset(keyBuffer, 0, sizeof(keyBuffer));
    snprintf(keyBuffer, sizeof(keyBuffer), "%s: ",key);
    char *ptr1 = NULL;
    char *ptr2 = NULL;
    do
    {
        ptr1 = strstr(header, keyBuffer);
        if(ptr1)
        {
            ptr2 = strstr(ptr1, "\x0D\x0A");
            len = ptr2 - ptr1;
            char *res = (char*)malloc(sizeof(char)*(len+1));
            if(res)
            {
                strncpy(res, ptr1, len);
                res[len] = '\0';
                snprintf(dest, maxlen, "%s", (res + strlen(keyBuffer)));
                free(res);
                ret = 1;
            }
        }
    }while(0);

    return ret;
}

static serverErrorCode_e getRequestUriPath(char*header, requestType_e type, char*dest, size_t maxlen)
{
    size_t len = 0;
    size_t ret = 0;
    char buffer[10];
    memset(buffer, 0, sizeof(buffer));
    char *ptr1 = NULL;
    char *ptr2 = NULL;

    do
    {
        ptr1 = strstr(header, " ");
        if(ptr1)
        {
            ptr2 = strstr(ptr1, " HTTP");
            len = ptr2 - ptr1;
            char *res = (char*)malloc(sizeof(char)*(len+1));
            if(res)
            {
                strncpy(res, ptr1, len);
                res[len] = '\0';
                memcpy(dest, res + strlen(typeStringTranslator[type]) + 1, maxlen);
                free(res);
                ret = 1;
            }
        }
    }while(0);
    return ret;
}

static httpVersion_e getHttpVersion(char*header)
{
    httpVersion_e version = http1_0;
    size_t len = 0;
    char buffer[10];

    char *ptr1 = NULL;
    char *ptr2 = NULL;
    ptr1 = strstr(header, "HTTP/");
    if(ptr1)
    {
        ptr2 = strstr(ptr1, "\x0D\x0A");
        len = ptr2 - ptr1;
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
            free(res);
        }
    }

    return version;
}

serverErrorCode_e serverInit(server_t *input, uint16_t port)
{
    if(input == NULL)
    {
        return errInvalidInput;
    }

    input->port = port;
    /**< 1. File descriptor soket : sockfd, dibuat global agar dapat di close oleh fungsi lain *///
    input->sockfd = -1;
    input->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (input->sockfd == -1)
    {
        printf("Socket creation failed...\n");
        return errCannotCreateSocket;
    }
    else printf("Success create socket at (%d)\n",input->sockfd);

    /**< 2. Assign IP, dan PORT yang akan digunakan untuk server *///
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
            printf("Socket bind failed...\n");
            close(input->sockfd);
            return errBindingFailed;
        }
        tryTimes++;
        sleep(1);
    }
    printf("Socket successfully binded..\n");
    return errSuccess;
}

serverErrorCode_e serverRead(server_t *input, request_t *output)
{
    if(input == NULL)
    {
        return errInvalidInput;
    }

    serverErrorCode_e ret = errSystemFailure;
    pthread_mutex_lock(&lockRead);
    do
    {
        /**< 
         * 1. Server Siap menerima siapapun yang request komunikasi dari client. 
         * Dalam hal ini, server hanya boleh menerima sebanyak totalConnectionAllowed 
         * yang akan dimasukkan dalam antrian di socket sockfd.
         *///
        if (listen(input->sockfd, totalConnectionAllowed) != 0)
        {
            ret = errCannotListenSocket;
            break;
        }

        struct sockaddr_in client;  
        memset(&client,0,sizeof(struct sockaddr_in));

        /**< 
         * 2. Terima konfigurasi, client addres dsb di variabel client
         *///
        socklen_t peerAddrSize = sizeof(client);
        input->clientfd = -1;
        input->clientfd = accept(input->sockfd, (struct sockaddr*)&client, &peerAddrSize);
        if (input->clientfd < 0)
        {
            ret = errCannotAcceptSocket;
            break;
        }

        struct in_addr ipAddr = client.sin_addr;
        memset(output->clientAddress,0,sizeof(output->clientAddress));
        inet_ntop(AF_INET, &ipAddr, output->clientAddress, INET_ADDRSTRLEN );

        /**< 
         * Baca data-data atau apapun yang dikirim dari client
         * Proses pembacaan satu persatu, sampai ketemu (0x0D,0x0A) atau
         * Sampai return read == 0 (yang artinya, end-of-file ,EOF) atau 
         * proses pembacaan sudah selesai di batch tersebut.
         *///
        int lenMessage = 0;
        int retval     = 0;
        char character = 0;
        char *message  = NULL;
        bool flagReadBody    = false;
        size_t contentLength = 0;
        size_t indexBody     = 0;

        /* Initialize */
        output->header  = NULL;
        output->content = NULL;
        output->message = NULL;
        message = malloc(DEFAULT_MESSAGE_ALLOCATION*sizeof(char));
        memset(message, 0, DEFAULT_MESSAGE_ALLOCATION);

        while(1)
        {
            retval = read(input->clientfd, &character, 1);
            if(retval == 0) break;
            else if(retval < 0)
            {
                printf("%s\n",strerror(errno));
                break;
            }

            message[lenMessage] = character;
            lenMessage += retval;
            message     = realloc(message, DEFAULT_MESSAGE_ALLOCATION+lenMessage);

            if((output->type == notKnown) && (strlen(message) >= MINIMUM_REQUEST_TYPE_LENGTH) && (strlen(message) < (MAXIMUM_REQUEST_TYPE_LENGTH + 1)) && (message[lenMessage] == ' '))
            {
                output->type = getRequestType(message);
            }

            if(output->type == post)
            {
                /* Finish read header */
                if(message[lenMessage-1] == 0x0A && message[lenMessage-2] == 0x0D && message[lenMessage-3] == 0x0A && message[lenMessage-4] == 0x0D)
                {
                    if(flagReadBody == false)
                    {
                        flagReadBody            = true;
                        output->header          = strdup(message);
                        indexBody               = strlen(message) - 1;
                        output->contentType     = getContentType(message);
                        contentLength           = getContentLength(output->header);
                        output->contentLength   = contentLength;
                        output->httpVersion     = getHttpVersion(message);
                        getHeaderKeyValue(message, "Host", output->host, sizeof(output->host));
                        getRequestUriPath(message, output->type, output->path, sizeof(output->path));
                        continue;
                    }
                }

                contentLength--;
                if(contentLength <= 0)
                {
                    printf("Payload (%lu): %s\n",indexBody, (message + indexBody));
                    output->content = strdup(message + indexBody);
                    break;
                }
            }
            else
            {
                /* Finish read header */
                if(message[lenMessage-1] == 0x0A && message[lenMessage-2] == 0x0D && message[lenMessage-3] == 0x0A && message[lenMessage-4] == 0x0D)
                {
                    output->header = strdup(message);
                    output->httpVersion = getHttpVersion(message);
                    getHeaderKeyValue(message, "Host", output->host, sizeof(output->host));
                    getRequestUriPath(message, output->type, output->path, sizeof(output->path));
                    break;
                }
            }
        }

        if(retval <= 0)
        {
            printf("Read failed...\n");
            close(input->clientfd);
            FREE(message);
            FREE(output->header);
            FREE(output->content);
            continue;
        }

        // printf("\n--------- From client ----------\n\n");
        // for(int i = 0 ; i < strlen(message) ; i++ ) printf("%c", message[i]);
        // printf("\n--------------------------------\n\n\n");

        output->message = strdup(message);
        FREE(message);
        ret = errSuccess;
    }while(0);
    pthread_mutex_unlock(&lockRead);
    return ret;
}

serverErrorCode_e serverSend(server_t *input, response_t *responseInput)
{
    if(input == NULL || responseInput == NULL)
    {
        return errInvalidInput;
    }

    if(responseInput->body == NULL)
    {
        return errInvalidBody;
    }

    serverErrorCode_e ret = errSuccess;
    char dtime[100];
    memset(dtime,0,sizeof(dtime));
    time_t now = time(0);
    struct tm *tm = gmtime(&now);
    strftime(dtime, sizeof(dtime), "%a, %d %b %Y %H:%M:%S %Z", tm);
    
    const char *template = "HTTP/1.1 200 OK\r\n"
                            "Server: sever-1.0\r\n"
                            "Accept-Ranges: none\r\n"
                            "Vary: Accept-Encoding\r\n"
                            "Last-Modified: %s\r\n"
                            "Connection: closed\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %u\r\n"
                            "\r\n"
                            "%s\n"
                            ;

    char *message = NULL;
    size_t lenBody = strlen(responseInput->body);
    size_t lenMessage = strlen(template) + strlen(dtime) + lenBody + 10;

    message = malloc((lenMessage)*sizeof(char));
    memset(message,0,lenMessage*sizeof(char));
    
    snprintf(message, lenMessage,
        template, 
        dtime, 
        lenBody,
        responseInput->body);
    serverSession++;

    int sendStatus = 0;
    sendStatus = send(input->clientfd, message, strlen(message), 0);
    close(input->clientfd);

    if(sendStatus > 0)
    {
        ret = errSuccess;
        // printf("\n----------- To client ----------\n\n");
        // for(int i = 0 ; i < strlen(message) ; i++ ) printf("%c", message[i]);
        // printf("\n--------------------------------\n\n");
    }
    else
    {
        ret = errCannotSendData;
    }

    FREE(message);
    return ret;
}

serverErrorCode_e serverReadFinish(request_t *input, response_t *responseInput)
{
    if(input == NULL)
    {
        return errInvalidInput;
    }

    FREE(input->message);
    FREE(input->header);
    FREE(input->content);
    memset(input, 0, sizeof(request_t));

    /* Free header if any */
    int i = 0;
    for(;i<responseInput->header.total;i++)
    {
        FREE(responseInput->header.data[i]);
    }
    FREE(responseInput->header.data);
    /* Free body */
    FREE(responseInput->body);
    memset(responseInput, 0, sizeof(response_t));

    return errSuccess;
}

serverErrorCode_e serverShutdown(server_t *input)
{
    if(input == NULL)
    {
        return errInvalidInput;
    }

    if(input->sockfd) close(input->sockfd);
    if(input->clientfd) close(input->clientfd);

    return errSuccess;
}

serverErrorCode_e serverAddCustomHeader(response_t*input, char *key, char*value)
{
    if(input == NULL || key == NULL || value == NULL)
    {
        return errInvalidInput;
    }

    char *buffer = NULL;
    size_t lenBuffer = strlen(key) + strlen(": ") + strlen(value) + strlen("\x0D\x0A") + 1;
    buffer = malloc(lenBuffer*sizeof(char));
    if(buffer == NULL)
    {
        return errSystemFailure;
    }

    memset(buffer,0,lenBuffer);
    snprintf(buffer, lenBuffer, "%s: %s\x0D\x0A",key,value);

    char **newHeader = NULL;
    input->header.total++;
    newHeader = calloc(input->header.total, sizeof(char*));
    if(newHeader == NULL)
    {
        FREE(buffer);
        return errSystemFailure;
    }

    size_t i = 0;
    for(;i < (input->header.total - 1);i++)
    {
        newHeader[i] = strdup(input->header.data[i]);
        FREE(input->header.data[i]);
    }
    newHeader[input->header.total] = strdup(buffer);
    FREE(buffer);

    FREE(input->header.data)
    input->header.data = newHeader;
    return errSuccess;
}

serverErrorCode_e serverAddContent(response_t *input, char *content, ...)
{
    if(input == NULL || content == NULL)
    {
        return errInvalidInput;
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
    input->body = strdup(bodyTemp);
    FREE(bodyTemp);
    return errSuccess;
}