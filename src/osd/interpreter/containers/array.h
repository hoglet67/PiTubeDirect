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

#ifndef BKTHOMPS_CONTAINERS_ARRAY_H
#define BKTHOMPS_CONTAINERS_ARRAY_H

#include "_bk_defines.h"

/**
 * The array data structure, which is a static contiguous array.
 */
typedef char *array;

/* Starting */
array array_init(size_t element_count, size_t data_size);

/* Utility */
size_t array_size(array me);
void array_copy_to_array(void *arr, array me);
void *array_get_data(array me);
bk_err array_add_all(array me, void *arr, size_t size);

/* Accessing */
bk_err array_set(array me, size_t index, void *data);
bk_err array_get(void *data, array me, size_t index);

/* Ending */
array array_destroy(array me);

#endif /* BKTHOMPS_CONTAINERS_ARRAY_H */
