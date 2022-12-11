/********************************************************************************
 * @file libcsvr_tls.h
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
#ifndef LIBCSV_TLS_H
#define LIBCSV_TLS_H

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct
{
    csvrServer_t *server;
    SSL_CTX* ctx;
}csvrTlsServer_t;

typedef struct{
    SSL *ssl;
    char *content;
}csvrTlsRequest_t;

csvrTlsServer_t* csvrTLSInit(uint16_t);
void csvrTlsShutdown(csvrTlsServer_t *server);
bool csvrCheckRoot();
void csvrTlsRead(csvrTlsServer_t* server, csvrTlsRequest_t*request);
void csvrTlsReadFinish(csvrTlsRequest_t* request);
void csvrTlsSend(csvrTlsRequest_t* request, char *content, size_t contentLength);

void csvrLoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile);
void csvrShowCertificate(SSL* ssl);
int csvrGenerateCertificate(char *name);

#endif