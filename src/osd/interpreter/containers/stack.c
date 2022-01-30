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

#include "deque.h"
#include "stack.h"

/**
 * Initializes a stack.
 *
 * @param data_size the size of each data element in the stack; must be
 *                  positive
 *
 * @return the newly-initialized stack, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
stack stack_init(const size_t data_size)
{
    return deque_init(data_size);
}

/**
 * Determines the size of the stack.
 *
 * @param me the stack to check size of
 *
 * @return the size of the stack
 */
size_t stack_size(stack me)
{
    return deque_size(me);
}

/**
 * Determines if the stack is empty, meaning it contains no elements.
 *
 * @param me the stack to check if empty
 *
 * @return BK_TRUE if the stack is empty, otherwise BK_FALSE
 */
bk_bool stack_is_empty(stack me)
{
    return deque_is_empty(me);
}

/**
 * Frees unused memory from the stack.
 *
 * @param me the stack to trim
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err stack_trim(stack me)
{
    return deque_trim(me);
}

/**
 * Copies the stack to an array. Since it is a copy, the array may be modified
 * without causing side effects to the stack data structure. Memory is not
 * allocated, thus the array being used for the copy must be allocated before
 * this function is called.
 *
 * @param arr the initialized array to copy the stack to
 * @param me  the stack to copy to the array
 */
void stack_copy_to_array(void *const arr, stack me)
{
    deque_copy_to_array(arr, me);
}

/**
 * Adds an element to the top of the stack. The pointer to the data being passed
 * in should point to the data type which this stack holds. For example, if this
 * stack holds integers, the data pointer should be a pointer to an integer.
 * Since the data is being copied, the pointer only has to be valid when this
 * function is called.
 *
 * @param me   the stack to add an element to
 * @param data the data to add to the stack
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 * @return -BK_ERANGE if size has reached representable limit
 */
bk_err stack_push(stack me, void *const data)
{
    return deque_push_back(me, data);
}

/**
 * Removes the top element of the stack, and copies the data which is being
 * removed. The pointer to the data being obtained should point to the data type
 * which this stack holds. For example, if this stack holds integers, the data
 * pointer should be a pointer to an integer. Since this data is being copied
 * from the array to the data pointer, the pointer only has to be valid when
 * this function is called.
 *
 * @param data the copy of the element being removed
 * @param me   the stack to remove the top element from
 *
 * @return BK_TRUE if the stack contained elements, otherwise BK_FALSE
 */
bk_bool stack_pop(void *const data, stack me)
{
    return deque_pop_back(data, me) == 0;
}

/**
 * Copies the top element of the stack. The pointer to the data being obtained
 * should point to the data type which this stack holds. For example, if this
 * stack holds integers, the data pointer should be a pointer to an integer.
 * Since this data is being copied from the array to the data pointer, the
 * pointer only has to be valid when this function is called.
 *
 * @param data the copy of the top element of the stack
 * @param me   the stack to copy from
 *
 * @return BK_TRUE if the stack contained elements, otherwise BK_FALSE
 */
bk_bool stack_top(void *const data, stack me)
{
    return deque_get_last(data, me) == 0;
}

/**
 * Clears the stack and sets it to the state from original initialization.
 *
 * @param me the stack to clear
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err stack_clear(stack me)
{
    return deque_clear(me);
}

/**
 * Frees the stack memory. Performing further operations after calling this
 * function results in undefined behavior. Freeing NULL is legal, and causes
 * no operation to be performed.
 *
 * @param me the stack to destroy
 *
 * @return NULL
 */
stack stack_destroy(stack me)
{
    return deque_destroy(me);
}
