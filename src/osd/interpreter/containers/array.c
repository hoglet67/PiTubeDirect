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
#include "array.h"

static const size_t book_keeping_size = sizeof(size_t);
static const size_t arr_size_offset = 0;
static const size_t data_size_offset = sizeof(size_t);
static const size_t data_ptr_offset = 2 * sizeof(size_t);

/**
 * Initializes an array.
 *
 * @param element_count the number of elements in the array; must not be
 *                      negative
 * @param data_size     the size of each element in the array; must be positive
 *
 * @return the newly-initialized array, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
array array_init(const size_t element_count, const size_t data_size)
{
    char *init;
    if (data_size == 0) {
        return NULL;
    }
    if (element_count * data_size / data_size != element_count) {
        return NULL;
    }
    if (data_ptr_offset + element_count * data_size < data_ptr_offset) {
        return NULL;
    }
    init = (char *)malloc(data_ptr_offset + element_count * data_size);
    if (!init) {
        return NULL;
    }
    memcpy(init + arr_size_offset, &element_count, book_keeping_size);
    memcpy(init + data_size_offset, &data_size, book_keeping_size);
    memset(init + data_ptr_offset, 0, element_count * data_size);
    return init;
}

/**
 * Gets the size of the array.
 *
 * @param me the array to check
 *
 * @return the size of the array
 */
size_t array_size(array me)
{
    size_t size;
    memcpy(&size, me + arr_size_offset, book_keeping_size);
    return size;
}

/**
 * Copies the array to a raw array. Since it is a copy, the raw array may be
 * modified without causing side effects to the array data structure. Memory
 * is not allocated, thus the raw array being used for the copy must be
 * allocated before this function is called. The size of the array should be
 * queried prior to calling this function, which also serves as the size of the
 * newly-copied raw array.
 *
 * @param arr the initialized raw array to copy the array to
 * @param me  the array to copy to the raw array
 */
void array_copy_to_array(void *const arr, array me)
{
    size_t element_count;
    size_t data_size;
    memcpy(&element_count, me + arr_size_offset, book_keeping_size);
    memcpy(&data_size, me + data_size_offset, book_keeping_size);
    memcpy(arr, me + data_ptr_offset, element_count * data_size);
}

/**
 * Gets the storage element of the array structure. The storage element is
 * contiguous in memory. The data pointer should be assigned to the correct
 * array type. For example, if the array holds integers, the data pointer should
 * be assigned to a raw integer array. The size of the array should be obtained
 * prior to calling this function, which also serves as the size of the queried
 * raw array. This pointer is not a copy, thus any modification to the data will
 * cause the array structure data to be modified. Operations using the array
 * functions may invalidate this pointer. The array owns this memory, thus it
 * should not be freed. If the array size if 0, this should not be used.
 *
 * @param me the array to get the storage element from
 *
 * @return the storage element of the array
 */
void *array_get_data(array me)
{
    size_t element_count;
    memcpy(&element_count, me + arr_size_offset, book_keeping_size);
    if (element_count == 0) {
        return NULL;
    }
    return me + data_ptr_offset;
}

/**
 * Copies elements from a raw array to the array. The size specifies the number
 * of elements to copy starting from the start of the raw array, which must be
 * less than or equal to the size of both the raw array and of the array. The
 * elements are copied to the array starting at the start of the array.
 *
 * @param me   the array to add data to
 * @param arr  the raw array to copy data from
 * @param size the number of elements to copy
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err array_add_all(array me, void *const arr, const size_t size)
{
    size_t element_count;
    size_t data_size;
    memcpy(&element_count, me + arr_size_offset, book_keeping_size);
    if (size > element_count) {
        return -BK_EINVAL;
    }
    memcpy(&data_size, me + data_size_offset, book_keeping_size);
    memcpy(me + data_ptr_offset, arr, size * data_size);
    return BK_OK;
}

/**
 * Sets the data for a specified element in the array. The pointer to the data
 * being passed in should point to the data type which this array holds. For
 * example, if this array holds integers, the data pointer should be a pointer
 * to an integer. Since the data is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me    the array to set data for
 * @param index the location to set data at in the array
 * @param data  the data to set at the location in the array
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err array_set(array me, const size_t index, void *const data)
{
    size_t element_count;
    size_t data_size;
    memcpy(&element_count, me + arr_size_offset, book_keeping_size);
    if (index >= element_count) {
        return -BK_EINVAL;
    }
    memcpy(&data_size, me + data_size_offset, book_keeping_size);
    memcpy(me + data_ptr_offset + index * data_size, data, data_size);
    return BK_OK;
}

/**
 * Copies the element at an index of the array to data. The pointer to the data
 * being obtained should point to the data type which this array holds. For
 * example, if this array holds integers, the data pointer should be a pointer
 * to an integer. Since this data is being copied from the array to the data
 * pointer, the pointer only has to be valid when this function is called.
 *
 *
 * @param data  the data to copy to
 * @param me    the array to copy from
 * @param index the index to copy from in the array
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err array_get(void *const data, array me, const size_t index)
{
    size_t element_count;
    size_t data_size;
    memcpy(&element_count, me + arr_size_offset, book_keeping_size);
    if (index >= element_count) {
        return -BK_EINVAL;
    }
    memcpy(&data_size, me + data_size_offset, book_keeping_size);
    memcpy(data, me + data_ptr_offset + index * data_size, data_size);
    return BK_OK;
}

/**
 * Frees the array memory. Performing further operations after calling this
 * function results in undefined behavior. Freeing NULL is legal, and causes
 * no operation to be performed.
 *
 * @param me the array to free from memory
 *
 * @return NULL
 */
array array_destroy(array me)
{
    free(me);
    return NULL;
}
