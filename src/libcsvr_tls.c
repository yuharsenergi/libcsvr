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
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "libcsvr.h"
#include "libcsvr_tls.h"

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
            printf("Cannot initialize SSL_CTX\n");
            free(tlsServer);
            abort();
        }
        csvrLoadCertificates(tlsServer->ctx, certificateKeyFile, privateKeyFile);
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

void csvrLoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        printf("Cannot use %s certificate file\n", CertFile);
        abort();
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        printf("Cannot use %s PrivateKey file\n", KeyFile);
        abort();
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        printf("Private key does not match the public certificate\n");
        abort();
    }
}

void csvrShowCertificate(SSL* ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
    {
        printf("No certificates.\n");
    }
}

csvrErrCode_e csvrTlsRead(csvrTlsServer_t* server, csvrTlsRequest_t*request)
{
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
        char buf[2048];
        memset(buf, 0, sizeof(buf));
        int  bytes;

        if ( SSL_accept(request->ssl) == -1)     /* do SSL-protocol accept */
        {
            printf("Cannot accept ssl socket \"%s\"\n", strerror(errno));
            ERR_print_errors_fp(stderr);
        }
        else
        {
            csvrShowCertificate(request->ssl);        /* get any certificates */
            bytes = SSL_read(request->ssl, buf, sizeof(buf)); /* get request */
            buf[bytes] = '\0';
            if ( bytes > 0 )
            {
                request->content = strdup(buf);
                ret = csvrSuccess;
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

void csvrTlsSend(csvrTlsRequest_t* request, char *content, size_t contentLength)
{
    SSL_write(request->ssl, content, contentLength); /* send reply */
}

void csvrTlsReadFinish(csvrTlsRequest_t* request)
{
    int clientfd = -1;
    clientfd = SSL_get_fd(request->ssl);    /* get socket connection */
    SSL_free(request->ssl);                 /* release SSL state */
    if(request->content) free(request->content);
    close(clientfd);                        /* close connection */
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
    return system(cmd);;
}