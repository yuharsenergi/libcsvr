#ifndef LIBCSVR_RESPONSE_H
#define LIBCSVR_RESPONSE_H

#include "libcsvr.h"

csvrErrCode_e createHttpErrorResponse(char **dest, csvrRequest_t * request, csvrHttpResponseCode_e code);

#endif