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

#ifndef BKTHOMPS_CONTAINERS_MULTIMAP_H
#define BKTHOMPS_CONTAINERS_MULTIMAP_H

#include "_bk_defines.h"

/**
 * The multimap data structure, which is a collection of key-value pairs, sorted
 * by keys.
 */
typedef struct internal_multimap *multimap;

/* Starting */
multimap multimap_init(size_t key_size, size_t value_size,
                       int (*key_comparator)(const void *const one,
                                             const void *const two),
                       int (*value_comparator)(const void *const one,
                                               const void *const two));

/* Capacity */
size_t multimap_size(multimap me);
bk_bool multimap_is_empty(multimap me);

/* Accessing */
bk_err multimap_put(multimap me, void *key, void *value);
void multimap_get_start(multimap me, void *key);
bk_bool multimap_get_next(void *value, multimap me);
size_t multimap_count(multimap me, void *key);
bk_bool multimap_contains(multimap me, void *key);
bk_bool multimap_remove(multimap me, void *key, void *value);
bk_bool multimap_remove_all(multimap me, void *key);

/* Retrieval */
void *multimap_first(multimap me);
void *multimap_last(multimap me);
void *multimap_lower(multimap me, void *key);
void *multimap_higher(multimap me, void *key);
void *multimap_floor(multimap me, void *key);
void *multimap_ceiling(multimap me, void *key);

/* Ending */
void multimap_clear(multimap me);
multimap multimap_destroy(multimap me);

#endif /* BKTHOMPS_CONTAINERS_MULTIMAP_H */
