/********************************************************************************
 * @file libcsvr_response.c
 * @ingroup libcsvr_response
 * @author Ergi (yuharsenergi@gmail.com)
 * @brief Collection functions of creating response payload for csvr
 * @version 0.1
 * @date 2022-12-06
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

#include "libcsvr_response.h"

csvrErrCode_e csvrCreateHttpErrorResponse(char **dest, csvrRequest_t * request, csvrHttpResponseCode_e code)
{
    if(dest == NULL)
    {
        return csvrInvalidInput;
    }

    int retprint = -1;
    char *typeStr[] = 
    {
        [csvrTypeNotKnown ] = "",
        [csvrTypeGet      ] = "GET ",
        [csvrTypePut      ] = "PUT ",
        [csvrTypePost     ] = "POST ",
        [csvrTypeHead     ] = "HEAD ",
        [csvrTypeDelete   ] = "DELETE ",
    };

    switch (code)
    {
    case csvrResponseNotFound:
        retprint = asprintf(dest,
            "<!DOCTYPE html>\n"
            "\t<html lang=\"en\">\n"
            "\t<head>\n"
            "\t\t<meta charset=\"utf-8\">\n"
            "\t\t<title>Error</title>\n"
            "\t</head>\n"
            "\t<body>\n"
            "\t\t<pre>Cannot %s%s</pre>\n"
            "\t</body>\n"
            "</html>\n",
            typeStr[request->type],
            request->path
        );
        break;
    case csvrResponseInternalServerError:
        retprint = asprintf(dest,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "\t<head>\n"
            "\t\t<meta charset=\"utf-8\">\n"
            "\t\t<title>Error</title>\n"
            "\t</head>\n"
            "\t<body>\n"
            "\t\t<pre>Internal Server Error</pre>\n"
            "\t</body>\n"
            "</html>\n"
        );
        break;
    default:
        retprint = 0;
        break;
    }

    if(retprint == -1)
    {
        return csvrSystemFailure;
    }
    else if(retprint == 0)
    {
        return csvrNotAnError;
    }
    return csvrSuccess;
}