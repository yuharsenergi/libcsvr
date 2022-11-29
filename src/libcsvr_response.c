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



csvrErrCode_e createHttpErrorResponse(char **dest, csvrRequest_t * request, csvrHttpResponseCode_e code)
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
            "<!DOCTYPE html>"
            "<html lang=\"en\">"
            "<head>"
            "<meta charset=\"utf-8\">"
            "<title>Error</title>"
            "</head>"
            "<body>"
            "<pre>Cannot %s%s</pre>"
            "</body>"
            "</html>",
            typeStr[request->type],
            request->path
        );
        break;
    case csvrResponseInternalServerError:
        retprint = asprintf(dest,
            "<!DOCTYPE html>"
            "<html lang=\"en\">"
            "<head>"
            "<meta charset=\"utf-8\">"
            "<title>Error</title>"
            "</head>"
            "<body>"
            "<pre>Internal Server Error</pre>"
            "</body>"
            "</html>"
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