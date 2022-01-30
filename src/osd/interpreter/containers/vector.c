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

#define BKTHOMPS_VECTOR_START_SPACE 8
#define BKTHOMPS_VECTOR_RESIZE_RATIO 1.5

struct internal_vector {
    size_t item_count;
    size_t item_capacity;
    size_t bytes_per_item;
    char *data;
};

/**
 * Initializes a vector.
 *
 * @param data_size the size of each element in the vector; must be positive
 *
 * @return the newly-initialized vector, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
vector vector_init(const size_t data_size)
{
    struct internal_vector *init;
    if (data_size == 0) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_vector));
    if (!init) {
        return NULL;
    }
    init->item_count = 0;
    init->item_capacity = BKTHOMPS_VECTOR_START_SPACE;
    init->bytes_per_item = data_size;
    if (init->item_capacity * data_size / data_size != init->item_capacity) {
        free(init);
        return NULL;
    }
    init->data = malloc(init->item_capacity * init->bytes_per_item);
    if (!init->data) {
        free(init);
        return NULL;
    }
    return init;
}

/**
 * Gets the size being used by the vector.
 *
 * @param me the vector to check
 *
 * @return the size being used by the vector
 */
size_t vector_size(vector me)
{
    return me->item_count;
}

/**
 * Gets the capacity that the internal storage of the vector is using.
 *
 * @param me the vector to check
 *
 * @return the capacity that the internal storage of the vector is using
 */
size_t vector_capacity(vector me)
{
    return me->item_capacity;
}

/**
 * Determines whether or not the vector is empty.
 *
 * @param me the vector to check
 *
 * @return BK_TRUE if the vector is empty, otherwise BK_FALSE
 */
bk_bool vector_is_empty(vector me)
{
    return vector_size(me) == 0;
}

/*
 * Sets the space of the buffer. Assumes that size is at least the same as the
 * number of items currently in the vector.
 */
static bk_err vector_set_space(vector me, const size_t size)
{
    char *temp;
    if (size * me->bytes_per_item / me->bytes_per_item != size) {
        return -BK_ERANGE;
    }
    temp = realloc(me->data, size * me->bytes_per_item);
    if (!temp) {
        return -BK_ENOMEM;
    }
    me->item_capacity = size;
    me->data = temp;
    return BK_OK;
}

/**
 * Reserves space specified. If more space than specified is already reserved,
 * then the previous space will be kept.
 *
 * @param me   the vector to reserve space for
 * @param size the space to reserve
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if space to reserve exceeds representable limit
 */
bk_err vector_reserve(vector me, size_t size)
{
    if (me->item_capacity >= size) {
        return BK_OK;
    }
    return vector_set_space(me, size);
}

/**
 * Sets the size of the vector buffer to the current size being used.
 *
 * @param me the vector to trim
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err vector_trim(vector me)
{
    return vector_set_space(me, me->item_count);
}

/**
 * Copies the vector to an array. Since it is a copy, the array may be modified
 * without causing side effects to the vector data structure. Memory is not
 * allocated, thus the array being used for the copy must be allocated before
 * this function is called. The size of the vector should be queried prior to
 * calling this function, which also serves as the size of the newly-copied
 * array.
 *
 * @param arr the initialized array to copy the vector to
 * @param me  the vector to copy to the array
 */
void vector_copy_to_array(void *const arr, vector me)
{
    memcpy(arr, me->data, me->item_count * me->bytes_per_item);
}

/**
 * Gets the storage element of the vector structure. The storage element is
 * contiguous in memory. The data pointer should be assigned to the correct
 * array type. For example, if the vector holds integers, the data pointer
 * should be assigned to an integer array. The size of the vector should be
 * obtained prior to calling this function, which also serves as the size of
 * the queried array. This pointer is not a copy, thus any modification to the
 * data will cause the vector structure data to be modified. Operations using
 * the vector functions may invalidate this pointer. The vector owns this
 * memory, thus it should not be freed. This should not be used if the vector
 * is empty.
 *
 * @param me the vector to get the storage element from
 *
 * @return the storage element of the vector
 */
void *vector_get_data(vector me)
{
    return me->data;
}

/**
 * Copies elements from an array to the vector. The size specifies the number of
 * elements to copy, starting from the beginning of the array. The size must be
 * less than or equal to the size of the array.
 *
 * @param me   the vector to add data to
 * @param arr  the array to copy data from
 * @param size the number of elements to copy
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err vector_add_all(vector me, void *const arr, const size_t size)
{
    const size_t cur_size = vector_size(me);
    size_t rc;
    if (size + cur_size < size) {
        return -BK_ERANGE;
    }
    rc = vector_reserve(me, size + cur_size);
    if (rc != BK_OK) {
        return rc;
    }
    memcpy(me->data + cur_size * me->bytes_per_item, arr,
           size * me->bytes_per_item);
    me->item_count += size;
    return BK_OK;
}

/**
 * Adds an element to the start of the vector. The pointer to the data being
 * passed in should point to the data type which this vector holds. For example,
 * if this vector holds integers, the data pointer should be a pointer to an
 * integer. Since the data is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me   the vector to add to
 * @param data the data to add to the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err vector_add_first(vector me, void *const data)
{
    return vector_add_at(me, 0, data);
}

/**
 * Adds an element to the location specified. The pointer to the data being
 * passed in should point to the data type which this vector holds. For example,
 * if this vector holds integers, the data pointer should be a pointer to an
 * integer. Since the data is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me    the vector to add to
 * @param index the location in the vector to add the data to
 * @param data  the data to add to the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_EINVAL if invalid argument
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err vector_add_at(vector me, const size_t index, void *const data)
{
    if (index > me->item_count) {
        return -BK_EINVAL;
    }
    if (me->item_count == me->item_capacity) {
        char *temp;
        const size_t item_limit = ((size_t) -1) / me->bytes_per_item;
        size_t new_space;
        if (item_limit == me->item_capacity) {
            return -BK_ERANGE;
        }
        new_space = me->item_capacity * BKTHOMPS_VECTOR_RESIZE_RATIO;
        if (new_space <= me->item_capacity) {
            new_space = item_limit;
        }
        temp = realloc(me->data, new_space * me->bytes_per_item);
        if (!temp) {
            return -BK_ENOMEM;
        }
        me->data = temp;
        me->item_capacity = new_space;
    }
    if (index != me->item_count) {
        memmove(me->data + (index + 1) * me->bytes_per_item,
                me->data + index * me->bytes_per_item,
                (me->item_count - index) * me->bytes_per_item);
    }
    memcpy(me->data + index * me->bytes_per_item, data, me->bytes_per_item);
    me->item_count++;
    return BK_OK;
}

/**
 * Adds an element to the end of the vector.
 *
 * @param me   the vector to add to
 * @param data the data to add to the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err vector_add_last(vector me, void *const data)
{
    return vector_add_at(me, me->item_count, data);
}

/**
 * Removes the first element from the vector.
 *
 * @param me the vector to remove from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_remove_first(vector me)
{
    return vector_remove_at(me, 0);
}

/**
 * Removes element based on its index.
 *
 * @param me    the vector to remove from
 * @param index the location in the vector to remove the data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_remove_at(vector me, const size_t index)
{
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    me->item_count--;
    memmove(me->data + index * me->bytes_per_item,
            me->data + (index + 1) * me->bytes_per_item,
            (me->item_count - index) * me->bytes_per_item);
    return BK_OK;
}

/**
 * Removes the last element from the vector.
 *
 * @param me the vector to remove from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_remove_last(vector me)
{
    if (me->item_count == 0) {
        return -BK_EINVAL;
    }
    me->item_count--;
    return BK_OK;
}

/**
 * Sets the data for the first element in the vector. The pointer to the data
 * being passed in should point to the data type which this vector holds. For
 * example, if this vector holds integers, the data pointer should be a pointer
 * to an integer. Since the data is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me   the vector to set data for
 * @param data the data to set at the start of the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_set_first(vector me, void *const data)
{
    return vector_set_at(me, 0, data);
}

/**
 * Sets the data for a specified element in the vector. The pointer to the data
 * being passed in should point to the data type which this vector holds. For
 * example, if this vector holds integers, the data pointer should be a pointer
 * to an integer. Since the data is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me    the vector to set data for
 * @param index the location to set data at in the vector
 * @param data  the data to set at the location in the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_set_at(vector me, const size_t index, void *const data)
{
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    memcpy(me->data + index * me->bytes_per_item, data, me->bytes_per_item);
    return BK_OK;
}

/**
 * Sets the data for the last element in the vector. The pointer to the data
 * being passed in should point to the data type which this vector holds. For
 * example, if this vector holds integers, the data pointer should be a pointer
 * to an integer. Since the data is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me   the vector to set data for
 * @param data the data to set at the end of the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_set_last(vector me, void *const data)
{
    return vector_set_at(me, me->item_count - 1, data);
}

/**
 * Copies the first element of the vector to data. The pointer to the data being
 * obtained should point to the data type which this vector holds. For example,
 * if this vector holds integers, the data pointer should be a pointer to an
 * integer. Since this data is being copied from the array to the data pointer,
 * the pointer only has to be valid when this function is called.
 *
 * @param data the data to copy to
 * @param me   the vector to copy from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_get_first(void *const data, vector me)
{
    return vector_get_at(data, me, 0);
}

/**
 * Copies the element at index of the vector to data. The pointer to the data
 * being obtained should point to the data type which this vector holds. For
 * example, if this vector holds integers, the data pointer should be a pointer
 * to an integer. Since this data is being copied from the array to the data
 * pointer, the pointer only has to be valid when this function is called.
 *
 * @param data  the data to copy to
 * @param me    the vector to copy from
 * @param index the index to copy from in the vector
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_get_at(void *const data, vector me, const size_t index)
{
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    memcpy(data, me->data + index * me->bytes_per_item, me->bytes_per_item);
    return BK_OK;
}

/**
 * Copies the last element of the vector to data. The pointer to the data being
 * obtained should point to the data type which this vector holds. For example,
 * if this vector holds integers, the data pointer should be a pointer to an
 * integer. Since this data is being copied from the array to the data pointer,
 * the pointer only has to be valid when this function is called.
 *
 * @param data the data to copy to
 * @param me   the vector to copy from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err vector_get_last(void *const data, vector me)
{
    return vector_get_at(data, me, me->item_count - 1);
}

/**
 * Clears the elements from the vector.
 *
 * @param me the vector to clear
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err vector_clear(vector me)
{
    me->item_count = 0;
    return vector_set_space(me, BKTHOMPS_VECTOR_START_SPACE);
}

/**
 * Frees the vector memory. Performing further operations after calling this
 * function results in undefined behavior. Freeing NULL is legal, and causes
 * no operation to be performed.
 *
 * @param me the vector to free from memory
 *
 * @return NULL
 */
vector vector_destroy(vector me)
{
    if (me) {
        free(me->data);
        free(me);
    }
    return NULL;
}
