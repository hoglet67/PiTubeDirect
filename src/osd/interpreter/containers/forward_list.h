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

#ifndef BKTHOMPS_CONTAINERS_FORWARD_LIST_H
#define BKTHOMPS_CONTAINERS_FORWARD_LIST_H

#include "_bk_defines.h"

/**
 * The forward_list data structure, which is a singly-linked list.
 */
typedef struct internal_forward_list *forward_list;

/* Starting */
forward_list forward_list_init(size_t data_size);

/* Utility */
size_t forward_list_size(forward_list me);
bk_bool forward_list_is_empty(forward_list me);
void forward_list_copy_to_array(void *arr, forward_list me);
bk_err forward_list_add_all(forward_list me, void *arr, size_t size);

/* Adding */
bk_err forward_list_add_first(forward_list me, void *data);
bk_err forward_list_add_at(forward_list me, size_t index, void *data);
bk_err forward_list_add_last(forward_list me, void *data);

/* Removing */
bk_err forward_list_remove_first(forward_list me);
bk_err forward_list_remove_at(forward_list me, size_t index);
bk_err forward_list_remove_last(forward_list me);

/* Setting */
bk_err forward_list_set_first(forward_list me, void *data);
bk_err forward_list_set_at(forward_list me, size_t index, void *data);
bk_err forward_list_set_last(forward_list me, void *data);

/* Getting */
bk_err forward_list_get_first(void *data, forward_list me);
bk_err forward_list_get_at(void *data, forward_list me, size_t index);
bk_err forward_list_get_last(void *data, forward_list me);

/* Ending */
void forward_list_clear(forward_list me);
forward_list forward_list_destroy(forward_list me);

#endif /* BKTHOMPS_CONTAINERS_FORWARD_LIST_H */
