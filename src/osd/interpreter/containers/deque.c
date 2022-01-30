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
#include "deque.h"

#define BKTHOMPS_DEQUE_MAX_BLOCK_BYTE_SIZE 4096
#define BKTHOMPS_DEQUE_MIN_BLOCK_ELEMENT_SIZE 16
#define BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT 8
#define BKTHOMPS_DEQUE_RESIZE_RATIO 1.5

struct internal_deque {
    size_t data_size;
    size_t block_size;
    size_t start_index;
    size_t end_index;
    size_t block_count;
    size_t alloc_block_start;
    size_t alloc_block_end;
    char **data;
};

/**
 * Initializes a deque.
 *
 * @param data_size the size of each element in the deque; must be positive
 *
 * @return the newly-initialized deque, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
deque deque_init(const size_t data_size)
{
    struct internal_deque *init;
    char *block;
    if (data_size == 0) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_deque));
    if (!init) {
        return NULL;
    }
    init->data_size = data_size;
    init->block_size = BKTHOMPS_DEQUE_MAX_BLOCK_BYTE_SIZE / init->data_size;
    if (init->block_size < BKTHOMPS_DEQUE_MIN_BLOCK_ELEMENT_SIZE) {
        init->block_size = BKTHOMPS_DEQUE_MIN_BLOCK_ELEMENT_SIZE;
    }
    if (init->block_size * data_size / data_size != init->block_size) {
        free(init);
        return NULL;
    }
    init->start_index =
            init->block_size * BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT / 2;
    init->end_index = init->start_index;
    init->block_count = BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT;
    init->alloc_block_start = init->start_index / init->block_size;
    init->alloc_block_end = init->alloc_block_start;
    init->data = malloc(init->block_count * sizeof(char *));
    if (!init->data) {
        free(init);
        return NULL;
    }
    block = malloc(init->block_size * init->data_size);
    if (!block) {
        free(init->data);
        free(init);
        return NULL;
    }
    init->data[init->alloc_block_start] = block;
    return init;
}

/**
 * Determines the size of the deque. The size is the number of data spaces being
 * used. The size starts at zero, and every time an element is added, it
 * increases by one.
 *
 * @param me the deque to check the size of
 *
 * @return the size of the deque
 */
size_t deque_size(deque me)
{
    return me->end_index - me->start_index;
}

/**
 * Determines if the deque is empty. It is empty if it has no elements.
 *
 * @param me the deque to check if empty
 *
 * @return BK_TRUE if the deque is empty, otherwise BK_FALSE
 */
bk_bool deque_is_empty(deque me)
{
    return deque_size(me) == 0;
}

/**
 * Trims the deque so that it does not use memory which does not need to be
 * used.
 *
 * @param me the deque to trim
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err deque_trim(deque me)
{
    size_t i;
    const size_t start_block_index = me->start_index / me->block_size;
    const size_t end_block_index = deque_is_empty(me) ? start_block_index :
                                   (me->end_index - 1) / me->block_size;
    const size_t updated_block_count = end_block_index - start_block_index + 1;
    char **updated_data = malloc(updated_block_count * sizeof(char *));
    if (!updated_data) {
        return -BK_ENOMEM;
    }
    memcpy(updated_data, me->data + start_block_index,
           updated_block_count * sizeof(char *));
    for (i = me->alloc_block_start; i < start_block_index; i++) {
        free(me->data[i]);
    }
    for (i = end_block_index + 1; i <= me->alloc_block_end; i++) {
        free(me->data[i]);
    }
    free(me->data);
    me->start_index -= start_block_index * me->block_size;
    me->end_index -= start_block_index * me->block_size;
    me->block_count = updated_block_count;
    me->alloc_block_start = 0;
    me->alloc_block_end = updated_block_count - 1;
    me->data = updated_data;
    return BK_OK;
}

/**
 * Copies the deque to an array. Since it is a copy, the array may be modified
 * without causing side effects to the deque data structure. Memory is not
 * allocated, thus the array being used for the copy must be allocated before
 * this function is called. The size of the deque should be queried prior to
 * calling this function, which also serves as the size of the newly-copied
 * array.
 *
 * @param arr the initialized array to copy the deque to
 * @param me  the deque to copy to the array
 */
void deque_copy_to_array(void *const arr, deque me)
{
    char *block;
    size_t i;
    size_t size_offset;
    const size_t start_block_index = me->start_index / me->block_size;
    const size_t start_inner_index = me->start_index % me->block_size;
    const size_t end_block_index = (me->end_index - 1) / me->block_size;
    const size_t end_inner_index = (me->end_index - 1) % me->block_size;
    const size_t first_block_length =
            me->block_size - start_inner_index < deque_size(me)
            ? me->block_size - start_inner_index : deque_size(me);
    if (deque_is_empty(me)) {
        return;
    }
    size_offset = first_block_length * me->data_size;
    memcpy(&block, me->data + start_block_index, sizeof(char *));
    memcpy(arr, block + start_inner_index * me->data_size, size_offset);
    for (i = start_block_index + 1; i < end_block_index; i++) {
        memcpy(&block, me->data + i, sizeof(char *));
        memcpy((char *) arr + size_offset, block,
               me->block_size * me->data_size);
        size_offset += me->block_size * me->data_size;
    }
    if (end_block_index != start_block_index) {
        memcpy(&block, me->data + end_block_index, sizeof(char *));
        memcpy((char *) arr + size_offset, block,
               (end_inner_index + 1) * me->data_size);
    }
}

/**
 * Copies elements from an array to the deque. The size specifies the number of
 * elements to copy, starting from the beginning of the array. The size must be
 * less than or equal to the size of the array.
 *
 * @param me   the deque to add data to
 * @param arr  the array to copy data from
 * @param size the number of elements to copy
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err deque_add_all(deque me, void *const arr, const size_t size)
{
    const size_t block_index = me->end_index / me->block_size;
    const size_t inner_index = me->end_index % me->block_size;
    const size_t first_block_space = me->block_size - inner_index;
    const size_t remaining_space = size - first_block_space;
    const size_t available_blocks = me->block_count - (block_index + 1);
    const size_t needed_blocks = 1 + remaining_space / me->block_size;
    size_t offset;
    size_t i;
    if (size <= first_block_space) {
        memcpy(me->data[block_index] + inner_index * me->data_size, arr,
               size * me->data_size);
        me->end_index += size;
        return BK_OK;
    }
    if (available_blocks < needed_blocks) {
        const size_t block_limit = ((size_t) -1) / me->block_size;
        const size_t appended_blocks = needed_blocks - available_blocks;
        const size_t new_block_count = me->block_count + appended_blocks;
        char **temp;
        if (new_block_count > block_limit) {
            return -BK_ERANGE;
        }
        temp = realloc(me->data, new_block_count * sizeof(char *));
        if (!temp) {
            return -BK_ENOMEM;
        }
        me->data = temp;
        me->block_count = new_block_count;
    }
    for (i = me->alloc_block_end + 1; i <= block_index + needed_blocks; i++) {
        me->data[i] = malloc(me->block_size * me->data_size);
        if (!me->data[i]) {
            return -BK_ENOMEM;
        }
        me->alloc_block_end++;
    }
    offset = first_block_space * me->data_size;
    memcpy(me->data[block_index] + inner_index * me->data_size, arr, offset);
    for (i = 1; i < needed_blocks; i++) {
        memcpy(me->data[block_index + i], (char *) arr + offset,
               me->block_size * me->data_size);
        offset += me->block_size * me->data_size;
    }
    memcpy(me->data[block_index + needed_blocks], (char *) arr + offset,
           (remaining_space % me->block_size) * me->data_size);
    me->end_index += size;
    return BK_OK;
}

/*
 * Returns the new block count after resize, or 0 on failure.
 */
static size_t deque_get_new_block_count(deque me)
{
    const size_t block_limit = ((size_t) -1) / me->block_size;
    size_t new_block_count;
    if (block_limit == me->block_count) {
        return 0;
    }
    new_block_count = me->block_count * BKTHOMPS_DEQUE_RESIZE_RATIO;
    if (new_block_count > block_limit || new_block_count <= me->block_count) {
        new_block_count = block_limit;
    }
    return new_block_count;
}

/**
 * Adds an element to the front of the deque. The pointer to the data being
 * passed in should point to the data type which this deque holds. For example,
 * if this deque holds integers, the data pointer should be a pointer to an
 * integer. Since the data is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me   the deque to add an element to
 * @param data the element to add
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err deque_push_front(deque me, void *const data)
{
    if (me->start_index == 0) {
        const size_t available_end = me->block_count - me->alloc_block_end - 1;
        if (available_end > BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT) {
            const size_t shift_end = available_end / 2;
            const size_t allocated_blocks =
                    me->alloc_block_end - me->alloc_block_start + 1;
            memmove(me->data + me->alloc_block_start + shift_end,
                    me->data + me->alloc_block_start,
                    allocated_blocks * sizeof(char *));
            me->start_index += shift_end * me->block_size;
            me->end_index += shift_end * me->block_size;
            me->alloc_block_start += shift_end;
            me->alloc_block_end += shift_end;
        } else {
            const size_t new_block_count = deque_get_new_block_count(me);
            size_t added_blocks;
            char **temp;
            if (new_block_count == 0) {
                return -BK_ERANGE;
            }
            added_blocks = new_block_count - me->block_count;
            temp = realloc(me->data, new_block_count * sizeof(char *));
            if (!temp) {
                return -BK_ENOMEM;
            }
            memmove(temp + added_blocks, temp,
                    me->block_count * sizeof(char *));
            me->data = temp;
            me->block_count = new_block_count;
            me->start_index += added_blocks * me->block_size;
            me->end_index += added_blocks * me->block_size;
            me->alloc_block_start += added_blocks;
            me->alloc_block_end += added_blocks;
        }
    }
    if (me->start_index % me->block_size == 0) {
        const size_t add_block_index = me->start_index / me->block_size - 1;
        if (add_block_index < me->alloc_block_start) {
            const size_t end_block = (me->end_index - 1) / me->block_size;
            if (end_block < me->alloc_block_end) {
                me->data[add_block_index] = me->data[me->alloc_block_end];
                me->alloc_block_end--;
            } else {
                me->data[add_block_index] =
                        malloc(me->block_size * me->data_size);
                if (!me->data[add_block_index]) {
                    return -BK_ENOMEM;
                }
            }
            me->alloc_block_start--;
        }
    }
    me->start_index--;
    {
        char *block;
        const size_t block_index = me->start_index / me->block_size;
        const size_t inner_index = me->start_index % me->block_size;
        memcpy(&block, me->data + block_index, sizeof(char *));
        memcpy(block + inner_index * me->data_size, data, me->data_size);
    }
    return BK_OK;
}

/**
 * Adds an element to the back of the deque. The pointer to the data being
 * passed in should point to the data type which this deque holds. For example,
 * if this deque holds integers, the data pointer should be a pointer to an
 * integer. Since the data is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me   the deque to add an element to
 * @param data the element to add
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err deque_push_back(deque me, void *const data)
{
    if (me->end_index == me->block_count * me->block_size) {
        const size_t available_start = me->alloc_block_start;
        if (available_start > BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT) {
            const size_t shift_start = available_start - available_start / 2;
            const size_t allocated_blocks =
                    me->alloc_block_end - me->alloc_block_start + 1;
            memmove(me->data + me->alloc_block_start / 2,
                    me->data + me->alloc_block_start,
                    allocated_blocks * sizeof(char *));
            me->start_index -= shift_start * me->block_size;
            me->end_index -= shift_start * me->block_size;
            me->alloc_block_start -= shift_start;
            me->alloc_block_end -= shift_start;
        } else {
            const size_t new_block_count = deque_get_new_block_count(me);
            char **temp;
            if (new_block_count == 0) {
                return -BK_ERANGE;
            }
            temp = realloc(me->data, new_block_count * sizeof(char *));
            if (!temp) {
                return -BK_ENOMEM;
            }
            me->data = temp;
            me->block_count = new_block_count;
        }
    }
    if (me->end_index % me->block_size == 0) {
        const size_t add_block_index = me->end_index / me->block_size;
        if (add_block_index > me->alloc_block_end) {
            const size_t start_block = me->start_index / me->block_size;
            if (start_block > me->alloc_block_start) {
                me->data[add_block_index] = me->data[me->alloc_block_start];
                me->alloc_block_start++;
            } else {
                me->data[add_block_index] =
                        malloc(me->block_size * me->data_size);
                if (!me->data[add_block_index]) {
                    return -BK_ENOMEM;
                }
            }
            me->alloc_block_end++;
        }
    }
    {
        char *block;
        const size_t block_index = me->end_index / me->block_size;
        const size_t inner_index = me->end_index % me->block_size;
        memcpy(&block, me->data + block_index, sizeof(char *));
        memcpy(block + inner_index * me->data_size, data, me->data_size);
    }
    me->end_index++;
    return BK_OK;
}

/**
 * Removes the front element from the deque and copies it to a data value. The
 * pointer to the data being obtained should point to the data type which this
 * deque holds. For example, if this deque holds integers, the data pointer
 * should be a pointer to an integer. Since this data is being copied from the
 * array to the data pointer, the pointer only has to be valid when this
 * function is called.
 *
 * @param data the value to copy to
 * @param me   the deque to remove from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_pop_front(void *const data, deque me)
{
    char *block;
    const size_t block_index = me->start_index / me->block_size;
    const size_t inner_index = me->start_index % me->block_size;
    if (deque_is_empty(me)) {
        return -BK_EINVAL;
    }
    memcpy(&block, me->data + block_index, sizeof(char *));
    memcpy(data, block + inner_index * me->data_size, me->data_size);
    me->start_index++;
    return BK_OK;
}

/**
 * Removes the back element from the deque and copies it to a data value. The
 * pointer to the data being obtained should point to the data type which this
 * deque holds. For example, if this deque holds integers, the data pointer
 * should be a pointer to an integer. Since this data is being copied from the
 * array to the data pointer, the pointer only has to be valid when this
 * function is called.
 *
 * @param data the value to copy to
 * @param me   the deque to remove from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_pop_back(void *const data, deque me)
{
    char *block;
    const size_t block_index = (me->end_index - 1) / me->block_size;
    const size_t inner_index = (me->end_index - 1) % me->block_size;
    if (deque_is_empty(me)) {
        return -BK_EINVAL;
    }
    memcpy(&block, me->data + block_index, sizeof(char *));
    memcpy(data, block + inner_index * me->data_size, me->data_size);
    me->end_index--;
    return BK_OK;
}

/**
 * Sets the first value of the deque. The pointer to the data being passed in
 * should point to the data type which this deque holds. For example, if this
 * deque holds integers, the data pointer should be a pointer to an integer.
 * Since the data is being copied, the pointer only has to be valid when this
 * function is called.
 *
 * @param me   the deque to set the value of
 * @param data the data to set
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_set_first(deque me, void *const data)
{
    return deque_set_at(me, 0, data);
}

/**
 * Sets the value of the deque at the specified index. The pointer to the data
 * being passed in should point to the data type which this deque holds. For
 * example, if this deque holds integers, the data pointer should be a pointer
 * to an integer. Since the data is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me    the deque to set the value of
 * @param index the index to set at
 * @param data  the data to set
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_set_at(deque me, size_t index, void *const data)
{
    char *block;
    const size_t block_index = (index + me->start_index) / me->block_size;
    const size_t inner_index = (index + me->start_index) % me->block_size;
    if (index >= deque_size(me)) {
        return -BK_EINVAL;
    }
    memcpy(&block, me->data + block_index, sizeof(char *));
    memcpy(block + inner_index * me->data_size, data, me->data_size);
    return BK_OK;
}

/**
 * Sets the last value of the deque. The pointer to the data being passed in
 * should point to the data type which this deque holds. For example, if this
 * deque holds integers, the data pointer should be a pointer to an integer.
 * Since the data is being copied, the pointer only has to be valid when this
 * function is called.
 *
 * @param me   the deque to set the value of
 * @param data the data to set
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_set_last(deque me, void *const data)
{
    return deque_set_at(me, deque_size(me) - 1, data);
}

/**
 * Gets the first value of the deque. The pointer to the data being obtained
 * should point to the data type which this deque holds. For example, if this
 * deque holds integers, the data pointer should be a pointer to an integer.
 * Since this data is being copied from the array to the data pointer, the
 * pointer only has to be valid when this function is called.
 *
 * @param data the data to set
 * @param me   the deque to set the value of
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_get_first(void *const data, deque me)
{
    return deque_get_at(data, me, 0);
}

/**
 * Gets the value of the deque at the specified index. The pointer to the data
 * being obtained should point to the data type which this deque holds. For
 * example, if this deque holds integers, the data pointer should be a pointer
 * to an integer. Since this data is being copied from the array to the data
 * pointer, the pointer only has to be valid when this function is called.
 *
 * @param data  the data to set
 * @param me    the deque to set the value of
 * @param index the index to set at
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_get_at(void *const data, deque me, size_t index)
{
    char *block;
    const size_t block_index = (index + me->start_index) / me->block_size;
    const size_t inner_index = (index + me->start_index) % me->block_size;
    if (index >= deque_size(me)) {
        return -BK_EINVAL;
    }
    memcpy(&block, me->data + block_index, sizeof(char *));
    memcpy(data, block + inner_index * me->data_size, me->data_size);
    return BK_OK;
}

/**
 * Gets the last value of the deque. The pointer to the data being obtained
 * should point to the data type which this deque holds. For example, if this
 * deque holds integers, the data pointer should be a pointer to an integer.
 * Since this data is being copied from the array to the data pointer, the
 * pointer only has to be valid when this function is called.
 *
 * @param data the data to set
 * @param me   the deque to set the value of
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err deque_get_last(void *const data, deque me)
{
    return deque_get_at(data, me, deque_size(me) - 1);
}

/**
 * Clears the deque and sets it to the original state from initialization.
 *
 * @param me the deque to clear
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err deque_clear(deque me)
{
    size_t i;
    char *updated_block;
    char **updated_data =
            malloc(BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT * sizeof(char *));
    if (!updated_data) {
        return -BK_ENOMEM;
    }
    updated_block = malloc(me->block_size * me->data_size);
    if (!updated_block) {
        free(updated_data);
        return -BK_ENOMEM;
    }
    for (i = me->alloc_block_start; i <= me->alloc_block_end; i++) {
        free(me->data[i]);
    }
    free(me->data);
    me->start_index = me->block_size * BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT / 2;
    me->end_index = me->start_index;
    me->block_count = BKTHOMPS_DEQUE_INITIAL_BLOCK_COUNT;
    me->alloc_block_start = me->start_index / me->block_size;
    me->alloc_block_end = me->alloc_block_start;
    me->data = updated_data;
    me->data[me->alloc_block_start] = updated_block;
    return BK_OK;
}

/**
 * Destroys the deque. Performing further operations after calling this function
 * results in undefined behavior. Freeing NULL is legal, and causes no operation
 * to be performed.
 *
 * @param me the deque to destroy
 *
 * @return NULL
 */
deque deque_destroy(deque me)
{
    if (me) {
        size_t i;
        for (i = me->alloc_block_start; i <= me->alloc_block_end; i++) {
            free(me->data[i]);
        }
        free(me->data);
        free(me);
    }
    return NULL;
}
