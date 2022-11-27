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

#define HEADER_CONTENT_LENGTH_KEY "Content-Length: "
#define HEADER_CONTENT_TYPE_KEY   "Content-Type: "
#define HEADER_SEPARATOR          "\x0D\x0A"
#define BODY_SEPARATOR            "\x0D\x0A\x0D\x0A"
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

int getContentLength(char*header, size_t headerLen)
{
    if(header == NULL)
    {
        return errInvalidInput;
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

static contentType_e getContentType(char*header)
{
    if(header == NULL)
    {
        return errInvalidInput;
    }

    contentType_e contentType = textHtml;
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

static size_t getHeaderKeyValue(char*header, char*key, char *dest, size_t maxlen)
{
    if(header == NULL || key == NULL || dest == NULL)
    {
        return errInvalidInput;
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
                    ret = 1;
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

static serverErrorCode_e getRequestUriPath(char*header, char*dest, size_t maxlen)
{
    if(header == NULL || dest == NULL)
    {
        return errInvalidInput;
    }

    serverErrorCode_e ret = errSystemFailure;
    char *buffer = NULL;
    size_t index = 0;
    size_t firstSpaceIndex = 0;
    size_t headerLen = strlen(header);
    while(index < headerLen)
    {
        /* Get the first space */
        if((header[index - 1] == ' ') && firstSpaceIndex == 0)
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
            ret = errSuccess;
            break;
        }
        index++;
    }

    if(index == headerLen)
    {
        ret = errInvalidHeader;
    }
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
    if(input == NULL || output == NULL)
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
            retval = read(input->clientfd, &character, 1);
            if(retval == 0) break;
            else if(retval < 0)
            {
                printf("%s\n",strerror(errno));
                break;
            }

            message[lenMessage] = character;
            lenMessage += (size_t)retval;
            message     = realloc(message, (size_t)(DEFAULT_MESSAGE_ALLOCATION+lenMessage));

            /* Finish read header */
            if(!memcmp(message + lenMessage - strlen(BODY_SEPARATOR), BODY_SEPARATOR, strlen(BODY_SEPARATOR)))
            {
                output->header = strdup(message);

                /* Get request type*/
                if(output->type == notKnown)
                {
                    output->type = getRequestType(message);
                }
                
                output->httpVersion = getHttpVersion(message);
                getHeaderKeyValue(output->header, "Host", output->host, sizeof(output->host));
                getRequestUriPath(output->header, output->path, sizeof(output->path));
                if(output->type == post)
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
                        output->content = strdup(message + indexBody);
                        break;
                    }
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

    ssize_t sendStatus = 0;
    sendStatus = send(input->clientfd, message, strlen(message), 0);
    close(input->clientfd);

    if(sendStatus > 0)
    {
        ret = errSuccess;
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

#ifdef TEST

/**
 * @brief Compile with :
 * 
 *    $ gcc src/libserver.c -Iinclude/ -o test -D_GNU_SOURCE -DTEST
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
    return 0;
}
#endif