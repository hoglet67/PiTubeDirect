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

#ifndef BKTHOMPS_CONTAINERS_PRIORITY_QUEUE_H
#define BKTHOMPS_CONTAINERS_PRIORITY_QUEUE_H

#include "_bk_defines.h"

/**
 * The priority_queue data structure, which adapts a container to provide a
 * priority queue. Adapts the vector container.
 */
typedef struct internal_priority_queue *priority_queue;

/* Starting */
priority_queue priority_queue_init(size_t data_size,
                                   int (*comparator)(const void *const one,
                                                     const void *const two));

/* Utility */
size_t priority_queue_size(priority_queue me);
bk_bool priority_queue_is_empty(priority_queue me);

/* Adding */
bk_err priority_queue_push(priority_queue me, void *data);

/* Removing */
bk_bool priority_queue_pop(void *data, priority_queue me);

/* Getting */
bk_bool priority_queue_front(void *data, priority_queue me);

/* Ending */
bk_err priority_queue_clear(priority_queue me);
priority_queue priority_queue_destroy(priority_queue me);

#endif /* BKTHOMPS_CONTAINERS_PRIORITY_QUEUE_H */
