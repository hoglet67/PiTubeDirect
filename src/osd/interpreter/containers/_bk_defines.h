/*
 * Copyright (c) 2017-2020 Bailey Thompson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BKTHOMPS_CONTAINERS_BK_DEFINES_H
#define BKTHOMPS_CONTAINERS_BK_DEFINES_H

#include <stdlib.h>

/*
 * Cannot use <errno.h> because the C89 standard does not guarantee all
 * of these. These are the same values as the regular linux error codes.
 */
#define BK_OK 0
#define BK_ENOMEM 12
#define BK_EINVAL 22
#define BK_ERANGE 34

/* Cannot use <stdbool.h> because it is C99 not C89. */
#define BK_FALSE 0
#define BK_TRUE (!BK_FALSE)

typedef int bk_err;
typedef int bk_bool;

#endif /* BKTHOMPS_CONTAINERS_BK_DEFINES_H */
