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

#ifndef BKTHOMPS_CONTAINERS_UNORDERED_MULTIMAP_H
#define BKTHOMPS_CONTAINERS_UNORDERED_MULTIMAP_H

#include "_bk_defines.h"

/**
 * The unordered_multimap data structure, which is a collection of key-value
 * pairs, hashed by keys.
 */
typedef struct internal_unordered_multimap *unordered_multimap;

/* Starting */
unordered_multimap
unordered_multimap_init(size_t key_size,
                        size_t value_size,
                        unsigned long (*hash)(const void *const key),
                        int (*key_comparator)(const void *const one,
                                              const void *const two),
                        int (*value_comparator)(const void *const one,
                                                const void *const two));

/* Utility */
bk_err unordered_multimap_rehash(unordered_multimap me);
size_t unordered_multimap_size(unordered_multimap me);
bk_bool unordered_multimap_is_empty(unordered_multimap me);

/* Accessing */
bk_err unordered_multimap_put(unordered_multimap me, void *key, void *value);
void unordered_multimap_get_start(unordered_multimap me, void *key);
bk_bool unordered_multimap_get_next(void *value, unordered_multimap me);
size_t unordered_multimap_count(unordered_multimap me, void *key);
bk_bool unordered_multimap_contains(unordered_multimap me, void *key);
bk_bool unordered_multimap_remove(unordered_multimap me,
                                  void *key, void *value);
bk_bool unordered_multimap_remove_all(unordered_multimap me, void *key);

/* Ending */
bk_err unordered_multimap_clear(unordered_multimap me);
unordered_multimap unordered_multimap_destroy(unordered_multimap me);

#endif /* BKTHOMPS_CONTAINERS_UNORDERED_MULTIMAP_H */
