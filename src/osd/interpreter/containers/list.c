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
#include "list.h"

struct internal_list {
    size_t bytes_per_item;
    size_t item_count;
    char *head;
    char *tail;
};

static const size_t ptr_size = sizeof(char *);
static const size_t node_next_ptr_offset = 0;
static const size_t node_prev_ptr_offset = sizeof(char *);
static const size_t node_data_ptr_offset = 2 * sizeof(char *);

/**
 * Initializes a doubly-linked list.
 *
 * @param data_size the size of data to store; must be positive
 *
 * @return the newly-initialized doubly-linked list, or NULL if it was not
 *         successfully initialized due to either invalid input arguments or
 *         memory allocation error
 */
list list_init(const size_t data_size)
{
    struct internal_list *init;
    if (data_size == 0) {
        return NULL;
    }
    if (node_data_ptr_offset + data_size < node_data_ptr_offset) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_list));
    if (!init) {
        return NULL;
    }
    init->bytes_per_item = data_size;
    init->item_count = 0;
    init->head = NULL;
    init->tail = NULL;
    return init;
}

/**
 * Gets the number of elements in the doubly-linked list.
 *
 * @param me the doubly-linked list to check
 *
 * @return the number of elements
 */
size_t list_size(list me)
{
    return me->item_count;
}

/**
 * Determines if the doubly-linked list is empty.
 *
 * @param me the doubly-linked list to check
 *
 * @return BK_TRUE if the list is empty, otherwise BK_FALSE
 */
bk_bool list_is_empty(list me)
{
    return list_size(me) == 0;
}

/**
 * Copies the nodes of the doubly-linked list to an array. Since it is a copy,
 * the array may be modified without causing side effects to the doubly-linked
 * list data structure. Memory is not allocated, thus the array being used for
 * the copy must be allocated before this function is called. The size of the
 * doubly-linked list should be queried prior to calling this function, which
 * also serves as the size of the newly-copied array.
 *
 * @param arr the initialized array to copy the doubly-linked list to
 * @param me  the doubly-linked list to copy to the array
 */
void list_copy_to_array(void *const arr, list me)
{
    char *traverse = me->head;
    size_t offset = 0;
    while (traverse) {
        memcpy((char *) arr + offset, traverse + node_data_ptr_offset,
               me->bytes_per_item);
        memcpy(&traverse, traverse + node_next_ptr_offset, ptr_size);
        offset += me->bytes_per_item;
    }
}

/**
 * Copies elements from an array to the doubly-linked list. The size specifies
 * the number of elements to copy, starting from the beginning of the array. The
 * size must be less than or equal to the size of the array.
 *
 * @param me   the doubly-linked list to add data to
 * @param arr  the array to copy data from
 * @param size the number of elements to copy
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err list_add_all(list me, void *const arr, const size_t size)
{
    size_t i;
    char *traverse_head;
    char *traverse;
    if (size == 0) {
        return BK_OK;
    }
    if (size + me->item_count < size) {
        return -BK_ERANGE;
    }
    traverse = malloc(2 * ptr_size + me->bytes_per_item);
    if (!traverse) {
        return -BK_ENOMEM;
    }
    traverse_head = traverse;
    memset(traverse + node_prev_ptr_offset, 0, ptr_size);
    memcpy(traverse + node_data_ptr_offset, arr, me->bytes_per_item);
    for (i = 1; i < size; i++) {
        char *node = malloc(2 * ptr_size + me->bytes_per_item);
        if (!node) {
            while (traverse) {
                char *backup = traverse;
                memcpy(&traverse, traverse + node_prev_ptr_offset, ptr_size);
                free(backup);
            }
            return -BK_ENOMEM;
        }
        memcpy(traverse + node_next_ptr_offset, &node, ptr_size);
        memcpy(node + node_prev_ptr_offset, &traverse, ptr_size);
        memcpy(node + node_data_ptr_offset,
               (char *) arr + i * me->bytes_per_item, me->bytes_per_item);
        traverse = node;
    }
    memset(traverse + node_next_ptr_offset, 0, ptr_size);
    if (list_is_empty(me)) {
        me->head = traverse_head;
    } else {
        memcpy(traverse_head + node_prev_ptr_offset, &me->tail, ptr_size);
        memcpy(me->tail + node_next_ptr_offset, &traverse_head, ptr_size);
    }
    me->tail = traverse;
    me->item_count += size;
    return BK_OK;
}

/*
 * Gets the node at index starting from the head.
 */
static char *list_get_node_from_head(list me, const size_t index)
{
    char *traverse = me->head;
    size_t i;
    for (i = 0; i < index; i++) {
        memcpy(&traverse, traverse + node_next_ptr_offset, ptr_size);
    }
    return traverse;
}

/*
 * Gets the node at index starting from the tail.
 */
static char *list_get_node_from_tail(list me, const size_t index)
{
    char *traverse = me->tail;
    size_t i;
    for (i = me->item_count - 1; i > index; i--) {
        memcpy(&traverse, traverse + node_prev_ptr_offset, ptr_size);
    }
    return traverse;
}

/*
 * Gets the node at the specified index.
 */
static char *list_get_node_at(list me, const size_t index)
{
    if (index < me->item_count / 2) {
        return list_get_node_from_head(me, index);
    }
    return list_get_node_from_tail(me, index);
}

/**
 * Adds data at the first index in the doubly-linked list. The pointer to the
 * data being passed in should point to the data type which this doubly-linked
 * list holds. For example, if this doubly-linked list holds integers, the data
 * pointer should be a pointer to an integer. Since the data is being copied,
 * the pointer only has to be valid when this function is called.
 *
 * @param me   the doubly-linked list to add data to
 * @param data the data to add to the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err list_add_first(list me, void *const data)
{
    return list_add_at(me, 0, data);
}

/**
 * Adds data at a specified index in the doubly-linked list. The pointer to the
 * data being passed in should point to the data type which this doubly-linked
 * list holds. For example, if this doubly-linked list holds integers, the data
 * pointer should be a pointer to an integer. Since the data is being copied,
 * the pointer only has to be valid when this function is called.
 *
 * @param me    the doubly-linked list to add data to
 * @param index the index to add the data at
 * @param data  the data to add to the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_add_at(list me, const size_t index, void *const data)
{
    char *node;
    if (index > me->item_count) {
        return -BK_EINVAL;
    }
    node = malloc(2 * ptr_size + me->bytes_per_item);
    if (!node) {
        return -BK_ENOMEM;
    }
    memcpy(node + node_data_ptr_offset, data, me->bytes_per_item);
    if (!me->head) {
        memset(node + node_next_ptr_offset, 0, ptr_size);
        memset(node + node_prev_ptr_offset, 0, ptr_size);
        me->head = node;
        me->tail = node;
    } else if (index == 0) {
        char *traverse = me->head;
        memcpy(traverse + node_prev_ptr_offset, &node, ptr_size);
        memcpy(node + node_next_ptr_offset, &traverse, ptr_size);
        memset(node + node_prev_ptr_offset, 0, ptr_size);
        me->head = node;
    } else if (index == me->item_count) {
        char *traverse = me->tail;
        memcpy(traverse + node_next_ptr_offset, &node, ptr_size);
        memset(node + node_next_ptr_offset, 0, ptr_size);
        memcpy(node + node_prev_ptr_offset, &traverse, ptr_size);
        me->tail = node;
    } else {
        char *traverse = list_get_node_at(me, index);
        char *traverse_prev;
        memcpy(&traverse_prev, traverse + node_prev_ptr_offset, ptr_size);
        memcpy(node + node_prev_ptr_offset, &traverse_prev, ptr_size);
        memcpy(node + node_next_ptr_offset, &traverse, ptr_size);
        memcpy(traverse_prev + node_next_ptr_offset, &node, ptr_size);
        memcpy(traverse + node_prev_ptr_offset, &node, ptr_size);
    }
    me->item_count++;
    return BK_OK;
}

/**
 * Adds data at the last index in the doubly-linked list. The pointer to the
 * data being passed in should point to the data type which this doubly-linked
 * list holds. For example, if this doubly-linked list holds integers, the data
 * pointer should be a pointer to an integer. Since the data is being copied,
 * the pointer only has to be valid when this function is called.
 *
 * @param me   the doubly-linked list to add data to
 * @param data the data to add to the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err list_add_last(list me, void *const data)
{
    return list_add_at(me, me->item_count, data);
}

/**
 * Removes the first piece of data from the doubly-linked list.
 *
 * @param me the doubly-linked list to remove data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_remove_first(list me)
{
    return list_remove_at(me, 0);
}

/**
 * Removes data from the doubly-linked list at the specified index.
 *
 * @param me    the doubly-linked list to remove data from
 * @param index the index to remove from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_remove_at(list me, const size_t index)
{
    char *traverse;
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    traverse = list_get_node_at(me, index);
    if (me->item_count == 1) {
        me->head = NULL;
        me->tail = NULL;
    } else if (index == 0) {
        char *traverse_next;
        memcpy(&traverse_next, traverse + node_next_ptr_offset, ptr_size);
        memset(traverse_next + node_prev_ptr_offset, 0, ptr_size);
        me->head = traverse_next;
    } else if (index == me->item_count - 1) {
        char *traverse_prev;
        memcpy(&traverse_prev, traverse + node_prev_ptr_offset, ptr_size);
        memset(traverse_prev + node_next_ptr_offset, 0, ptr_size);
        me->tail = traverse_prev;
    } else {
        char *traverse_next;
        char *traverse_prev;
        memcpy(&traverse_next, traverse + node_next_ptr_offset, ptr_size);
        memcpy(&traverse_prev, traverse + node_prev_ptr_offset, ptr_size);
        memcpy(traverse_next + node_prev_ptr_offset, &traverse_prev, ptr_size);
        memcpy(traverse_prev + node_next_ptr_offset, &traverse_next, ptr_size);
    }
    free(traverse);
    me->item_count--;
    return BK_OK;
}

/**
 * Removes the last piece of data from the doubly-linked list.
 *
 * @param me the doubly-linked list to remove data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_remove_last(list me)
{
    return list_remove_at(me, me->item_count - 1);
}

/**
 * Sets the data at the first index in the doubly-linked list. The pointer to
 * the data being passed in should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since the data is being
 * copied, the pointer only has to be valid when this function is called.
 *
 * @param me   the doubly-linked list to set data for
 * @param data the data to set in the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_set_first(list me, void *const data)
{
    return list_set_at(me, 0, data);
}

/**
 * Sets the data at the specified index in the doubly-linked list. The pointer
 * to the data being passed in should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since the data is being
 * copied, the pointer only has to be valid when this function is called.
 *
 * @param me    the doubly-linked list to set data for
 * @param index the index to set data in the doubly-linked list
 * @param data  the data to set in the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_set_at(list me, const size_t index, void *const data)
{
    char *traverse;
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    traverse = list_get_node_at(me, index);
    memcpy(traverse + node_data_ptr_offset, data, me->bytes_per_item);
    return BK_OK;
}

/**
 * Sets the data at the last index in the doubly-linked list. The pointer to
 * the data being passed in should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since the data is being
 * copied, the pointer only has to be valid when this function is called.
 *
 * @param me   the doubly-linked list to set data for
 * @param data the data to set in the doubly-linked list
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_set_last(list me, void *const data)
{
    return list_set_at(me, me->item_count - 1, data);
}

/**
 * Gets the data at the first index in the doubly-linked list. The pointer to
 * the data being obtained should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since this data is being
 * copied from the array to the data pointer, the pointer only has to be valid
 * when this function is called.
 *
 * @param data the data to get
 * @param me   the doubly-linked list to get data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_get_first(void *const data, list me)
{
    return list_get_at(data, me, 0);
}

/**
 * Gets the data at the specified index in the doubly-linked list. The pointer
 * to the data being obtained should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since this data is being
 * copied from the array to the data pointer, the pointer only has to be valid
 * when this function is called.
 *
 * @param data  the data to get
 * @param me    the doubly-linked list to get data from
 * @param index the index to get data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_get_at(void *const data, list me, const size_t index)
{
    char *traverse;
    if (index >= me->item_count) {
        return -BK_EINVAL;
    }
    traverse = list_get_node_at(me, index);
    memcpy(data, traverse + node_data_ptr_offset, me->bytes_per_item);
    return BK_OK;
}

/**
 * Gets the data at the last index in the doubly-linked list. The pointer to
 * the data being obtained should point to the data type which this doubly-
 * linked list holds. For example, if this doubly-linked list holds integers,
 * the data pointer should be a pointer to an integer. Since this data is being
 * copied from the array to the data pointer, the pointer only has to be valid
 * when this function is called.
 *
 * @param data the data to get
 * @param me   the doubly-linked list to get data from
 *
 * @return  BK_OK     if no error
 * @return -BK_EINVAL if invalid argument
 */
bk_err list_get_last(void *const data, list me)
{
    return list_get_at(data, me, me->item_count - 1);
}

/**
 * Clears all elements from the doubly-linked list.
 *
 * @param me the doubly-linked list to clear
 */
void list_clear(list me)
{
    char *traverse = me->head;
    while (traverse) {
        char *temp = traverse;
        memcpy(&traverse, traverse + node_next_ptr_offset, ptr_size);
        free(temp);
    }
    me->item_count = 0;
    me->head = NULL;
    me->tail = NULL;
}

/**
 * Destroys the doubly-linked list. Performing further operations after calling
 * this function results in undefined behavior. Freeing NULL is legal, and
 * causes no operation to be performed.
 *
 * @param me the doubly-linked list to destroy
 *
 * @return NULL
 */
list list_destroy(list me)
{
    if (me) {
        list_clear(me);
        free(me);
    }
    return NULL;
}
