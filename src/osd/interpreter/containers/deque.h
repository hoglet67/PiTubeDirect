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

#ifndef BKTHOMPS_CONTAINERS_DEQUE_H
#define BKTHOMPS_CONTAINERS_DEQUE_H

#include "_bk_defines.h"

/**
 * The deque data structure, which is a doubly-ended queue.
 */
typedef struct internal_deque *deque;

/* Starting */
deque deque_init(size_t data_size);

/* Utility */
size_t deque_size(deque me);
bk_bool deque_is_empty(deque me);
bk_err deque_trim(deque me);
void deque_copy_to_array(void *arr, deque me);
bk_err deque_add_all(deque me, void *arr, size_t size);

/* Adding */
bk_err deque_push_front(deque me, void *data);
bk_err deque_push_back(deque me, void *data);

/* Removing */
bk_err deque_pop_front(void *data, deque me);
bk_err deque_pop_back(void *data, deque me);

/* Setting */
bk_err deque_set_first(deque me, void *data);
bk_err deque_set_at(deque me, size_t index, void *data);
bk_err deque_set_last(deque me, void *data);

/* Getting */
bk_err deque_get_first(void *data, deque me);
bk_err deque_get_at(void *data, deque me, size_t index);
bk_err deque_get_last(void *data, deque me);

/* Ending */
bk_err deque_clear(deque me);
deque deque_destroy(deque me);

#endif /* BKTHOMPS_CONTAINERS_DEQUE_H */
