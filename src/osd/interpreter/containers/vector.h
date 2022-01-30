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

#ifndef BKTHOMPS_CONTAINERS_VECTOR_H
#define BKTHOMPS_CONTAINERS_VECTOR_H

#include "_bk_defines.h"

/**
 * The vector data structure, which is a dynamic contiguous array.
 */
typedef struct internal_vector *vector;

/* Starting */
vector vector_init(size_t data_size);

/* Utility */
size_t vector_size(vector me);
size_t vector_capacity(vector me);
bk_bool vector_is_empty(vector me);
bk_err vector_reserve(vector me, size_t size);
bk_err vector_trim(vector me);
void vector_copy_to_array(void *arr, vector me);
void *vector_get_data(vector me);
bk_err vector_add_all(vector me, void *arr, size_t size);

/* Adding */
bk_err vector_add_first(vector me, void *data);
bk_err vector_add_at(vector me, size_t index, void *data);
bk_err vector_add_last(vector me, void *data);

/* Removing */
bk_err vector_remove_first(vector me);
bk_err vector_remove_at(vector me, size_t index);
bk_err vector_remove_last(vector me);

/* Setting */
bk_err vector_set_first(vector me, void *data);
bk_err vector_set_at(vector me, size_t index, void *data);
bk_err vector_set_last(vector me, void *data);

/* Getting */
bk_err vector_get_first(void *data, vector me);
bk_err vector_get_at(void *data, vector me, size_t index);
bk_err vector_get_last(void *data, vector me);

/* Ending */
bk_err vector_clear(vector me);
vector vector_destroy(vector me);

#endif /* BKTHOMPS_CONTAINERS_VECTOR_H */
