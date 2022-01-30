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

#include <string.h>
#include "vector.h"
#include "priority_queue.h"

struct internal_priority_queue {
    size_t data_size;
    int (*comparator)(const void *const one, const void *const two);
    vector data;
};

/**
 * Initializes a priority queue.
 *
 * @param data_size  the size of the data in the priority queue; must be
 *                   positive
 * @param comparator the priority comparator function; must not be NULL
 *
 * @return the newly-initialized priority queue, or NULL if it was not
 *         successfully initialized due to either invalid input arguments or
 *         memory allocation error
 */
priority_queue priority_queue_init(const size_t data_size,
                                   int (*comparator)(const void *const,
                                                     const void *const))
{
    struct internal_priority_queue *init;
    if (data_size == 0 || !comparator) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_priority_queue));
    if (!init) {
        return NULL;
    }
    init->data_size = data_size;
    init->comparator = comparator;
    init->data = vector_init(data_size);
    if (!init->data) {
        free(init);
        return NULL;
    }
    return init;
}

/**
 * Gets the size of the priority queue.
 *
 * @param me the priority queue to check
 *
 * @return the size of the priority queue
 */
size_t priority_queue_size(priority_queue me)
{
    return vector_size(me->data);
}

/**
 * Determines whether or not the priority queue is empty.
 *
 * @param me the priority queue to check
 *
 * @return BK_TRUE if the priority queue is empty, otherwise BK_FALSE
 */
bk_bool priority_queue_is_empty(priority_queue me)
{
    return vector_is_empty(me->data);
}

/**
 * Adds an element to the priority queue. The pointer to the data being passed
 * in should point to the data type which this priority queue holds. For
 * example, if this priority queue holds integers, the data pointer should be a
 * pointer to an integer. Since the data is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me   the priority queue to add an element to
 * @param data the data to add to the queue
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err priority_queue_push(priority_queue me, void *const data)
{
    bk_err rc;
    char *vector_storage;
    size_t index;
    size_t parent_index;
    char *data_index;
    char *data_parent_index;
    char *const temp = malloc(me->data_size);
    if (!temp) {
        return -BK_ENOMEM;
    }
    rc = vector_add_last(me->data, data);
    if (rc != BK_OK) {
        free(temp);
        return rc;
    }
    vector_storage = vector_get_data(me->data);
    index = vector_size(me->data) - 1;
    parent_index = (index - 1) / 2;
    data_index = vector_storage + index * me->data_size;
    data_parent_index = vector_storage + parent_index * me->data_size;
    while (index > 0 && me->comparator(data_index, data_parent_index) > 0) {
        memcpy(temp, data_parent_index, me->data_size);
        memcpy(data_parent_index, data_index, me->data_size);
        memcpy(data_index, temp, me->data_size);
        index = parent_index;
        parent_index = (index - 1) / 2;
        data_index = vector_storage + index * me->data_size;
        data_parent_index = vector_storage + parent_index * me->data_size;
    }
    free(temp);
    return BK_OK;
}

/**
 * Removes the highest priority element from the priority queue. The pointer to
 * the data being obtained should point to the data type which this priority
 * queue holds. For example, if this priority queue holds integers, the data
 * pointer should be a pointer to an integer. Since this data is being copied
 * from the array to the data pointer, the pointer only has to be valid when
 * this function is called.
 *
 * @param data the data to have copied from the priority queue
 * @param me   the priority queue to pop the next element from
 *
 * @return BK_TRUE if the priority queue contained elements, otherwise BK_FALSE
 */
bk_bool priority_queue_pop(void *const data, priority_queue me)
{
    char *vector_storage;
    size_t size;
    char *temp;
    size_t index;
    size_t left_index;
    size_t right_index;
    char *data_index;
    char *data_left_index;
    char *data_right_index;
    const bk_err rc = vector_get_first(data, me->data);
    if (rc != BK_OK) {
        return BK_FALSE;
    }
    vector_storage = vector_get_data(me->data);
    size = vector_size(me->data) - 1;
    temp = vector_storage + size * me->data_size;
    memcpy(vector_storage, temp, me->data_size);
    left_index = 1;
    right_index = 2;
    data_index = vector_storage;
    data_left_index = vector_storage + left_index * me->data_size;
    data_right_index = vector_storage + right_index * me->data_size;
    for (;;) {
        if (right_index < size &&
            me->comparator(data_right_index, data_left_index) > 0 &&
            me->comparator(data_right_index, data_index) > 0) {
            /* Swap parent and right child then continue down right child. */
            memcpy(temp, data_index, me->data_size);
            memcpy(data_index, data_right_index, me->data_size);
            memcpy(data_right_index, temp, me->data_size);
            index = right_index;
        } else if (left_index < size &&
                   me->comparator(data_left_index, data_index) > 0) {
            /* Swap parent and left child then continue down left child. */
            memcpy(temp, data_index, me->data_size);
            memcpy(data_index, data_left_index, me->data_size);
            memcpy(data_left_index, temp, me->data_size);
            index = left_index;
        } else {
            break;
        }
        left_index = 2 * index + 1;
        right_index = 2 * index + 2;
        data_index = vector_storage + index * me->data_size;
        data_left_index = vector_storage + left_index * me->data_size;
        data_right_index = vector_storage + right_index * me->data_size;
    }
    vector_remove_last(me->data);
    return BK_TRUE;
}

/**
 * Gets the highest priority element in the priority queue. The pointer to the
 * data being obtained should point to the data type which this priority queue
 * holds. For example, if this priority queue holds integers, the data pointer
 * should be a pointer to an integer. Since this data is being copied from the
 * array to the data pointer, the pointer only has to be valid when this
 * function is called.
 *
 * @param data the out copy of the highest priority element in the priority
 *             queue
 * @param me   the priority queue to copy from
 *
 * @return BK_TRUE if the priority queue contained elements, otherwise BK_FALSE
 */
bk_bool priority_queue_front(void *const data, priority_queue me)
{
    return vector_get_first(data, me->data) == 0;
}

/**
 * Clears the elements from the priority queue.
 *
 * @param me the priority queue to clear
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err priority_queue_clear(priority_queue me)
{
    return vector_clear(me->data);
}

/**
 * Frees the priority queue memory. Performing further operations after calling
 * this function results in undefined behavior. Freeing NULL is legal, and
 * causes no operation to be performed.
 *
 * @param me the priority queue to free from memory
 *
 * @return NULL
 */
priority_queue priority_queue_destroy(priority_queue me)
{
    if (me) {
        vector_destroy(me->data);
        free(me);
    }
    return NULL;
}
