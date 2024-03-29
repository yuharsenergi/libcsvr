/********************************************************************************
 * @file libcsvr_tls.c
 * @author Ergi (yuharsenergi@gmail.com)
 * @brief Collection functions of all the csvr main runtime functions with TLS
 * @version 0.1
 * @date 2022-12-11
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
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#ifdef WITH_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "libcsvr.h"
#include "libcsvr_internal.h"
#include "libcsvr_tls.h"

static char *csvrTlsConvertError(int error);

bool csvrCheckRoot()
{
    if (getuid() != 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

csvrTlsServer_t* csvrTLSInit(uint16_t port, char *certificateKeyFile, char*privateKeyFile)
{
    csvrTlsServer_t *tlsServer = NULL;
    tlsServer = calloc(1, sizeof(csvrTlsServer_t));
    if(tlsServer)
    {
        // Initialize the SSL library
        SSL_library_init();
        // SSL_METHOD *method;
        OpenSSL_add_all_algorithms();  /* load & register all cryptos, etc. */
        SSL_load_error_strings();   /* load all error messages */
        
        /* SSL Context */
        #if OPENSSL_VERSION_NUMBER < 0x10100000L
        tlsServer->ctx = SSL_CTX_new(SSLv23_server_method());   /* create new context from method */
        #else
        tlsServer->ctx = SSL_CTX_new(TLS_server_method());   /* create new context from method */
        #endif
        if (tlsServer->ctx == NULL )
        {
            printf("[ERROR] Cannot initialize SSL_CTX\n");
            free(tlsServer);
            return NULL;
        }

        if(csvrLoadCertificates(tlsServer->ctx, certificateKeyFile, privateKeyFile) != csvrSuccess)
        {
            free(tlsServer);
            return NULL;
        }

        printf("[DEBUG] Load certificate key success : %s\n",certificateKeyFile);
        printf("[DEBUG] Load private key success     : %s\n",privateKeyFile);
        
        tlsServer->server = csvrInit(port);
        if(tlsServer->server == NULL)
        {
            SSL_CTX_free(tlsServer->ctx);
            free(tlsServer);
            return NULL;
        }

        /* 1. Listen to socket */
        if (listen(tlsServer->server->sockfd, 5) != 0)
        {
            SSL_CTX_free(tlsServer->ctx);
            csvrShutdown(tlsServer->server);
            free(tlsServer);
            return NULL;
        }
    }
    return tlsServer;
}

void csvrTlsShutdown(csvrTlsServer_t *server)
{
    if(server)
    {
        SSL_CTX_free(server->ctx);
        csvrShutdown(server->server);
    }
}

csvrErrCode_e csvrLoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        printf("[ERROR] Cannot use %s certificate file\n", CertFile);
        return csvrSystemFailure;
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        printf("[ERROR] Cannot use %s PrivateKey file\n", KeyFile);
        return csvrSystemFailure;
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        printf("[ERROR] Private key does not match the public certificate\n");
        return csvrSystemFailure;
    }
    return csvrSuccess;
}

void csvrShowCertificate(SSL* ssl)
{
    if(ssl == NULL) return;

    X509 *cert = NULL;
    char *line = NULL;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
    if ( cert != NULL )
    {
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("[INFO] Client certificate name: %s\n", line ? line : "null");
        CSVR_FREE(line);

        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("[INFO] Client certificate Issuer: %s\n", line ? line : "null");
        CSVR_FREE(line);

        X509_free(cert);
    }
    else
    {
        printf("[DEBUG] No client certificate provided.\n");
    }
}

csvrErrCode_e csvrTlsRead(csvrTlsServer_t* server, csvrTlsRequest_t*request)
{
    if(server == NULL || request == NULL)
    {
        return csvrInvalidInput;
    }

    if(server->server == NULL)
    {
        return csvrInvalidInput;
    }

    csvrErrCode_e ret = 0;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int clientfd = -1;
    clientfd = accept(server->server->sockfd, (struct sockaddr*)&addr, &len);  /* accept connection as usual */
    memcpy(request->address, inet_ntoa(addr.sin_addr), sizeof(request->address));
    request->port = ntohs(addr.sin_port);

    request->ssl = SSL_new(server->ctx);              /* get new SSL state with context */
    if(request->ssl)
    {
        SSL_set_fd(request->ssl, clientfd);      /* set connection socket to SSL state */
        char buf[16000];
        memset(buf, 0, sizeof(buf));
        int  bytes;

        if ( SSL_accept(request->ssl) == -1)     /* do SSL-protocol accept */
        {
            printf("[ERROR] Cannot accept ssl socket \"%s\"\n", errno ? strerror(errno) : "");
            ERR_print_errors_fp(stderr);
        }
        else
        {
            csvrShowCertificate(request->ssl);        /* get any certificates */
            bytes = SSL_read(request->ssl, buf, sizeof(buf) - 1); /* get request */
            buf[bytes] = '\0';
            if ( bytes > 0 )
            {
                /* Initialize */
                request->data.header  = NULL;
                request->data.content = NULL;
                request->data.message = NULL;

                /* Get the full payload (header + body (if any)) */
                request->data.content = strdup(buf);
                if(request->data.content == NULL)
                {
                    return csvrSystemFailure;
                }

                /* Get the header payload only */
                char *header = NULL;
                int headerSize = csvrGetHeaderFromPayload(&header, buf, bytes);
                if(header == NULL)
                {
                    CSVR_FREE(request->data.content);
                    return csvrSystemFailure;
                }
                /* save the header pointer here */
                request->data.header = header;

                /* get the contentLength from the payload */
                request->data.contentLength = csvrGetContentLength(request->data.header, strlen(request->data.header));
                /* If no content-length in the header, the count manually */
                if(request->data.contentLength == 0)
                {
                    request->data.contentLength = bytes - headerSize;
                }

                /* get the request type from the payload */
                request->data.type = getRequestType(buf);
                request->data.contentType = csvrGetContentType(header);

                /* Get the body payload only */
                char *body = NULL;
                if(request->data.type == csvrTypePost)
                {
                    int contentLength = request->data.contentLength + 1;
                    body = malloc(contentLength*sizeof(char));
                    if(body == NULL)
                    {
                        CSVR_FREE(request->data.content);
                        CSVR_FREE(request->data.header);
                        return csvrSystemFailure;
                    }
                    else
                    {
                        memset(body, 0, contentLength);
                        memcpy(body, buf + headerSize, contentLength - 1);
                        body[contentLength] = '\0';
                        ret = csvrSuccess;
                    }
                }
                /* save the pointer here */
                request->data.message = body;
            }
            else
            {
                ERR_print_errors_fp(stderr);
                ret = csvrSystemFailure;
            }
        }
    }
    return ret;
}

csvrErrCode_e csvrTlsSend(csvrTlsServer_t *server, csvrTlsRequest_t* request, char *content, size_t contentLength)
{

    if(server == NULL || request == NULL || content == NULL || contentLength == 0)
    {
        return csvrInvalidInput;
    }

    if(server->server == NULL)
    {
        return csvrInvalidInput;
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
        server->server->serverName,
        dtime, 
        contentLength,
        content);

    if(retprint == -1)
    {
        return csvrSystemFailure;
    }

    int sendStatus = 0;
    sendStatus = SSL_write(request->ssl, payload, strlen(payload)); /* send reply */

    if(sendStatus > 0)
    {
        ret = csvrSuccess;
    }
    else
    {
        int sslError = 0;
        sslError = SSL_get_error(request->ssl, sendStatus);
        printf("[DEBUG] Error send data: (%d) %s\n",sslError, csvrTlsConvertError(sslError));
        if(ret == 0)
        {
            printf("[DEBUG] EOF was observed that violates the protocol.\n");
        }
        else if(ret == -1)
        {
            sslError = errno;
            printf("[DEBUG] Errno (%d) %s\n",sslError, strerror(sslError));
        }
        ret = csvrFailedSendData;
    }

    CSVR_FREE(payload);
    return ret;
}

void csvrTlsReadFinish(csvrTlsRequest_t* request)
{
    int clientfd = -1;
    clientfd = SSL_get_fd(request->ssl);    /* get socket connection */
    close(clientfd);                        /* close connection */
    SSL_free(request->ssl);                 /* release SSL state */
    CSVR_FREE(request->data.content)
    CSVR_FREE(request->data.message)
    CSVR_FREE(request->data.header)
}

int csvrGenerateCertificate(char *name)
{
    int ret = 0;
    char*cmd = NULL;
    ret = asprintf(&cmd,"openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout certificate/%s.pem -out certificate/%s.pem", name, name);
    if(ret == -1)
    {
        return ret;
    }
    return system(cmd);
}

static char *csvrTlsConvertError(int error)
{
    switch (error)
    {
    /* No error occured */
    case SSL_ERROR_NONE:
        return " ";
    case SSL_ERROR_SSL:
        return "SSL_ERROR_ZERO_RETURN";
    case SSL_ERROR_WANT_READ:
        return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_WRITE:
        return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_ZERO_RETURN:
        printf("[ERROR] Maybe the TLS/SSL connection has been closed.\n");
        return "SSL_ERROR_ZERO_RETURN";
    case SSL_ERROR_WANT_CONNECT:
        return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT:
        return "SSL_ERROR_WANT_ACCEPT";
    case SSL_ERROR_WANT_X509_LOOKUP:
        return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_SYSCALL:
        return "";
    default:
        break;
    }
    return "SSL ERROR UNKNOWN";
}
#endif //end WITH_TLS