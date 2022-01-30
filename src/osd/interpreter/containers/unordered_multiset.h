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

#ifndef BKTHOMPS_CONTAINERS_UNORDERED_MULTISET_H
#define BKTHOMPS_CONTAINERS_UNORDERED_MULTISET_H

#include "_bk_defines.h"

/**
 * The unordered_multiset data structure, which is a collection of keys, hashed
 * by keys.
 */
typedef struct internal_unordered_multiset *unordered_multiset;

/* Starting */
unordered_multiset
unordered_multiset_init(size_t key_size,
                        unsigned long (*hash)(const void *const key),
                        int (*comparator)(const void *const one,
                                          const void *const two));

/* Utility */
bk_err unordered_multiset_rehash(unordered_multiset me);
size_t unordered_multiset_size(unordered_multiset me);
bk_bool unordered_multiset_is_empty(unordered_multiset me);

/* Accessing */
bk_err unordered_multiset_put(unordered_multiset me, void *key);
size_t unordered_multiset_count(unordered_multiset me, void *key);
bk_bool unordered_multiset_contains(unordered_multiset me, void *key);
bk_bool unordered_multiset_remove(unordered_multiset me, void *key);
bk_bool unordered_multiset_remove_all(unordered_multiset me, void *key);

/* Ending */
bk_err unordered_multiset_clear(unordered_multiset me);
unordered_multiset unordered_multiset_destroy(unordered_multiset me);

#endif /* BKTHOMPS_CONTAINERS_UNORDERED_MULTISET_H */
