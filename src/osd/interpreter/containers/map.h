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

#ifndef BKTHOMPS_CONTAINERS_MAP_H
#define BKTHOMPS_CONTAINERS_MAP_H

#include "_bk_defines.h"

/**
 * The map data structure, which is a collection of key-value pairs, sorted by
 * keys, keys are unique.
 */
typedef struct internal_map *map;

/* Starting */
map map_init(size_t key_size, size_t value_size,
             int (*comparator)(const void *const one, const void *const two));

/* Capacity */
size_t map_size(map me);
bk_bool map_is_empty(map me);

/* Accessing */
bk_err map_put(map me, void *key, void *value);
bk_bool map_get(void *value, map me, void *key);
bk_bool map_contains(map me, void *key);
bk_bool map_remove(map me, void *key);

/* Retrieval */
void *map_first(map me);
void *map_last(map me);
void *map_lower(map me, void *key);
void *map_higher(map me, void *key);
void *map_floor(map me, void *key);
void *map_ceiling(map me, void *key);

/* Ending */
void map_clear(map me);
map map_destroy(map me);

#endif /* BKTHOMPS_CONTAINERS_MAP_H */
