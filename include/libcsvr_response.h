/********************************************************************************
 * @file libcsvr_response.h
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
#ifndef LIBCSVR_RESPONSE_H
#define LIBCSVR_RESPONSE_H

#include "libcsvr.h"

/***************************************************************************************************************
 * @brief Function to create response payload based on given csvrHttpResponseCode_e
 * 
 * @param[out] dest pointer to char variable. This variable will be dynamically allocated and must be freed if
 *                  user already finished using the variable.
 * @param[in] request pointer to csvrRequest_t data structure which contains request type and path information.
 * @param[in] code the desired HTTP response code.
 * @return This function return one of the following value : 
 *          csvrInvalidInput,
 *          csvrNotAnError,
 *          csvrSystemFailure,
 *          csvrSuccess
 ***************************************************************************************************************/
csvrErrCode_e csvrResponseGenerateHTMLContent(char **dest, csvrRequest_t * request, csvrHttpResponseCode_e code);

#endif