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
#include "multiset.h"

struct internal_multiset {
    size_t size;
    size_t key_size;
    int (*comparator)(const void *const one, const void *const two);
    char *root;
};

static const size_t ptr_size = sizeof(char *);
static const size_t count_size = sizeof(size_t);
/* Node balance is always the first byte (at index 0). */
static const size_t node_count_offset = sizeof(signed char);
static const size_t node_parent_offset = 1 + sizeof(size_t);
static const size_t node_left_child_offset =
        1 + sizeof(size_t) + sizeof(char *);
static const size_t node_right_child_offset =
        1 + sizeof(size_t) + 2 * sizeof(char *);
static const size_t node_key_offset = 1 + sizeof(size_t) + 3 * sizeof(char *);

/**
 * Initializes a multi-set.
 *
 * @param key_size   the size of each element in the multi-set; must be
 *                   positive
 * @param comparator the comparator function used for key ordering; must not be
 *                   NULL
 *
 * @return the newly-initialized multi-set, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
multiset multiset_init(const size_t key_size,
                       int (*const comparator)(const void *const,
                                               const void *const))
{
    struct internal_multiset *init;
    if (key_size == 0 || !comparator) {
        return NULL;
    }
    if (node_key_offset + key_size < node_key_offset) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_multiset));
    if (!init) {
        return NULL;
    }
    init->size = 0;
    init->key_size = key_size;
    init->comparator = comparator;
    init->root = NULL;
    return init;
}

/**
 * Gets the size of the multi-set.
 *
 * @param me the multi-set to check
 *
 * @return the size of the multi-set
 */
size_t multiset_size(multiset me)
{
    return me->size;
}

/**
 * Determines whether or not the multi-set is empty.
 *
 * @param me the multi-set to check
 *
 * @return BK_TRUE if the multi-set is empty, otherwise BK_FALSE
 */
bk_bool multiset_is_empty(multiset me)
{
    return multiset_size(me) == 0;
}

/*
 * Resets the parent reference.
 */
static void multiset_reference_parent(multiset me, char *const parent,
                                      char *const child)
{
    char *grand_parent;
    char *grand_parent_left_child;
    memcpy(child + node_parent_offset, parent + node_parent_offset, ptr_size);
    memcpy(&grand_parent, parent + node_parent_offset, ptr_size);
    if (!grand_parent) {
        me->root = child;
        return;
    }
    memcpy(&grand_parent_left_child, grand_parent + node_left_child_offset,
           ptr_size);
    if (grand_parent_left_child == parent) {
        memcpy(grand_parent + node_left_child_offset, &child, ptr_size);
    } else {
        memcpy(grand_parent + node_right_child_offset, &child, ptr_size);
    }
}

/*
 * Rotates the AVL tree to the left.
 */
static void multiset_rotate_left(multiset me, char *const parent,
                                 char *const child)
{
    char *left_grand_child;
    multiset_reference_parent(me, parent, child);
    memcpy(&left_grand_child, child + node_left_child_offset, ptr_size);
    if (left_grand_child) {
        memcpy(left_grand_child + node_parent_offset, &parent, ptr_size);
    }
    memcpy(parent + node_parent_offset, &child, ptr_size);
    memcpy(parent + node_right_child_offset, &left_grand_child, ptr_size);
    memcpy(child + node_left_child_offset, &parent, ptr_size);
}

/*
 * Rotates the AVL tree to the right.
 */
static void multiset_rotate_right(multiset me, char *const parent,
                                  char *const child)
{
    char *right_grand_child;
    multiset_reference_parent(me, parent, child);
    memcpy(&right_grand_child, child + node_right_child_offset, ptr_size);
    if (right_grand_child) {
        memcpy(right_grand_child + node_parent_offset, &parent, ptr_size);
    }
    memcpy(parent + node_parent_offset, &child, ptr_size);
    memcpy(parent + node_left_child_offset, &right_grand_child, ptr_size);
    memcpy(child + node_right_child_offset, &parent, ptr_size);
}

/*
 * Performs a left repair.
 */
static char *multiset_repair_left(multiset me, char *const parent,
                                  char *const child)
{
    multiset_rotate_left(me, parent, child);
    if (child[0] == 0) {
        parent[0] = 1;
        child[0] = -1;
    } else {
        parent[0] = 0;
        child[0] = 0;
    }
    return child;
}

/*
 * Performs a right repair.
 */
static char *multiset_repair_right(multiset me, char *const parent,
                                   char *const child)
{
    multiset_rotate_right(me, parent, child);
    if (child[0] == 0) {
        parent[0] = -1;
        child[0] = 1;
    } else {
        parent[0] = 0;
        child[0] = 0;
    }
    return child;
}

/*
 * Performs a left-right repair.
 */
static char *multiset_repair_left_right(multiset me, char *const parent,
                                        char *const child,
                                        char *const grand_child)
{
    multiset_rotate_left(me, child, grand_child);
    multiset_rotate_right(me, parent, grand_child);
    if (grand_child[0] == 1) {
        parent[0] = 0;
        child[0] = -1;
    } else if (grand_child[0] == 0) {
        parent[0] = 0;
        child[0] = 0;
    } else {
        parent[0] = 1;
        child[0] = 0;
    }
    grand_child[0] = 0;
    return grand_child;
}

/*
 * Performs a right-left repair.
 */
static char *multiset_repair_right_left(multiset me, char *const parent,
                                        char *const child,
                                        char *const grand_child)
{
    multiset_rotate_right(me, child, grand_child);
    multiset_rotate_left(me, parent, grand_child);
    if (grand_child[0] == 1) {
        parent[0] = -1;
        child[0] = 0;
    } else if (grand_child[0] == 0) {
        parent[0] = 0;
        child[0] = 0;
    } else {
        parent[0] = 0;
        child[0] = 1;
    }
    grand_child[0] = 0;
    return grand_child;
}

/*
 * Repairs the AVL tree on insert. The only possible values of parent->balance
 * are {-2, 2} and the only possible values of child->balance are {-1, 0, 1}.
 */
static char *multiset_repair(multiset me, char *const parent,
                             char *const child, char *const grand_child)
{
    if (parent[0] == 2) {
        if (child[0] == -1) {
            return multiset_repair_right_left(me, parent, child, grand_child);
        }
        return multiset_repair_left(me, parent, child);
    }
    if (child[0] == 1) {
        return multiset_repair_left_right(me, parent, child, grand_child);
    }
    return multiset_repair_right(me, parent, child);
}

/*
 * Balances the AVL tree on insert.
 */
static void multiset_insert_balance(multiset me, char *const item)
{
    char *grand_child = NULL;
    char *child = item;
    char *parent;
    memcpy(&parent, item + node_parent_offset, ptr_size);
    while (parent) {
        char *parent_left;
        memcpy(&parent_left, parent + node_left_child_offset, ptr_size);
        if (parent_left == child) {
            parent[0]--;
        } else {
            parent[0]++;
        }
        /* If balance is zero after modification, then the tree is balanced. */
        if (parent[0] == 0) {
            return;
        }
        /* Must re-balance if not in {-1, 0, 1} */
        if (parent[0] > 1 || parent[0] < -1) {
            /* After one repair, the tree is balanced. */
            multiset_repair(me, parent, child, grand_child);
            return;
        }
        grand_child = child;
        child = parent;
        memcpy(&parent, parent + node_parent_offset, ptr_size);
    }
}

/*
 * Creates and allocates a node.
 */
static char *multiset_create_node(multiset me, const void *const data,
                                  char *const parent)
{
    char *insert = malloc(1 + count_size + 3 * ptr_size + me->key_size);
    const size_t one = 1;
    if (!insert) {
        return NULL;
    }
    insert[0] = 0;
    memcpy(insert + node_count_offset, &one, count_size);
    memcpy(insert + node_parent_offset, &parent, ptr_size);
    memset(insert + node_left_child_offset, 0, ptr_size);
    memset(insert + node_right_child_offset, 0, ptr_size);
    memcpy(insert + node_key_offset, data, me->key_size);
    me->size++;
    return insert;
}

/**
 * Adds a key to the multi-set. The pointer to the key being passed in should
 * point to the key type which this multi-set holds. For example, if this
 * multi-set holds key integers, the key pointer should be a pointer to an
 * integer. Since the key is being copied, the pointer only has to be valid
 * when this function is called.
 *
 * @param me  the multi-set to add to
 * @param key the key to add
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_bool multiset_put(multiset me, void *const key)
{
    char *traverse;
    if (!me->root) {
        char *insert = multiset_create_node(me, key, NULL);
        if (!insert) {
            return -BK_ENOMEM;
        }
        me->root = insert;
        return BK_OK;
    }
    traverse = me->root;
    for (;;) {
        const int compare = me->comparator(key, traverse + node_key_offset);
        if (compare < 0) {
            char *traverse_left;
            memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
            if (traverse_left) {
                traverse = traverse_left;
            } else {
                char *insert = multiset_create_node(me, key, traverse);
                if (!insert) {
                    return -BK_ENOMEM;
                }
                memcpy(traverse + node_left_child_offset, &insert, ptr_size);
                multiset_insert_balance(me, insert);
                return BK_OK;
            }
        } else if (compare > 0) {
            char *traverse_right;
            memcpy(&traverse_right, traverse + node_right_child_offset,
                   ptr_size);
            if (traverse_right) {
                traverse = traverse_right;
            } else {
                char *insert = multiset_create_node(me, key, traverse);
                if (!insert) {
                    return -BK_ENOMEM;
                }
                memcpy(traverse + node_right_child_offset, &insert, ptr_size);
                multiset_insert_balance(me, insert);
                return BK_OK;
            }
        } else {
            size_t count;
            memcpy(&count, traverse + node_count_offset, count_size);
            count++;
            memcpy(traverse + node_count_offset, &count, count_size);
            me->size++;
            return BK_OK;
        }
    }
}

/*
 * If a match occurs, returns the match. Else, returns NULL.
 */
static char *multiset_equal_match(multiset me, const void *const key)
{
    char *traverse = me->root;
    if (!traverse) {
        return NULL;
    }
    for (;;) {
        const int compare = me->comparator(key, traverse + node_key_offset);
        if (compare < 0) {
            char *traverse_left;
            memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
            if (traverse_left) {
                traverse = traverse_left;
            } else {
                return NULL;
            }
        } else if (compare > 0) {
            char *traverse_right;
            memcpy(&traverse_right, traverse + node_right_child_offset,
                   ptr_size);
            if (traverse_right) {
                traverse = traverse_right;
            } else {
                return NULL;
            }
        } else {
            return traverse;
        }
    }
}

/**
 * Determines the count of a specific key in the multi-set. The pointer to the
 * key being passed in should point to the key type which this multi-set holds.
 * For example, if this multi-set holds key integers, the key pointer should be
 * a pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to check for the count
 * @param key the key to check
 *
 * @return the count of a specific key in the multi-set
 */
size_t multiset_count(multiset me, void *const key)
{
    const char *const item = multiset_equal_match(me, key);
    size_t item_count;
    if (!item) {
        return 0;
    }
    memcpy(&item_count, item + node_count_offset, count_size);
    return item_count;
}

/**
 * Determines if the multi-set contains the specified key. The pointer to the
 * key being passed in should point to the key type which this multi-set holds.
 * For example, if this multi-set holds key integers, the key pointer should be
 * a pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to check for the key
 * @param key the key to check
 *
 * @return BK_TRUE if the multiset contained the key, otherwise BK_FALSE
 */
bk_bool multiset_contains(multiset me, void *const key)
{
    return multiset_equal_match(me, key) != NULL;
}

/*
 * Repairs the AVL tree by pivoting on an item.
 */
static char *multiset_repair_pivot(multiset me, char *const item,
                                   const int is_left_pivot)
{
    char *child;
    char *grand_child;
    char *item_right;
    char *item_left;
    char *child_right;
    char *child_left;
    memcpy(&item_right, item + node_right_child_offset, ptr_size);
    memcpy(&item_left, item + node_left_child_offset, ptr_size);
    child = is_left_pivot ? item_right : item_left;
    memcpy(&child_right, child + node_right_child_offset, ptr_size);
    memcpy(&child_left, child + node_left_child_offset, ptr_size);
    grand_child = child[0] == 1 ? child_right : child_left;
    return multiset_repair(me, item, child, grand_child);
}

/*
 * Goes back up the tree repairing it along the way.
 */
static void multiset_trace_ancestors(multiset me, char *item)
{
    char *child = item;
    char *parent;
    memcpy(&parent, item + node_parent_offset, ptr_size);
    while (parent) {
        char *parent_left;
        memcpy(&parent_left, parent + node_left_child_offset, ptr_size);
        if (parent_left == child) {
            parent[0]++;
        } else {
            parent[0]--;
        }
        /* The tree is balanced if balance is -1 or +1 after modification. */
        if (parent[0] == -1 || parent[0] == 1) {
            return;
        }
        /* Must re-balance if not in {-1, 0, 1} */
        if (parent[0] > 1 || parent[0] < -1) {
            child = multiset_repair_pivot(me, parent, parent_left == child);
            memcpy(&parent, child + node_parent_offset, ptr_size);
            /* If balance is -1 or +1 after modification or   */
            /* the parent is NULL, then the tree is balanced. */
            if (!parent || child[0] == -1 || child[0] == 1) {
                return;
            }
        } else {
            child = parent;
            memcpy(&parent, parent + node_parent_offset, ptr_size);
        }
    }
}

/*
 * Balances the AVL tree on deletion.
 */
static void multiset_delete_balance(multiset me, char *item,
                                    const int is_left_deleted)
{
    if (is_left_deleted) {
        item[0]++;
    } else {
        item[0]--;
    }
    /* If balance is -1 or +1 after modification, then the tree is balanced. */
    if (item[0] == -1 || item[0] == 1) {
        return;
    }
    /* Must re-balance if not in {-1, 0, 1} */
    if (item[0] > 1 || item[0] < -1) {
        char *item_parent;
        item = multiset_repair_pivot(me, item, is_left_deleted);
        memcpy(&item_parent, item + node_parent_offset, ptr_size);
        if (!item_parent || item[0] == -1 || item[0] == 1) {
            return;
        }
    }
    multiset_trace_ancestors(me, item);
}

/*
 * Removes traverse when it has no children.
 */
static void multiset_remove_no_children(multiset me, const char *const traverse)
{
    char *traverse_parent;
    char *traverse_parent_left;
    memcpy(&traverse_parent, traverse + node_parent_offset, ptr_size);
    /* If no parent and no children, then the only node is traverse. */
    if (!traverse_parent) {
        me->root = NULL;
        return;
    }
    memcpy(&traverse_parent_left, traverse_parent + node_left_child_offset,
           ptr_size);
    /* No re-reference needed since traverse has no children. */
    if (traverse_parent_left == traverse) {
        memset(traverse_parent + node_left_child_offset, 0, ptr_size);
        multiset_delete_balance(me, traverse_parent, 1);
    } else {
        memset(traverse_parent + node_right_child_offset, 0, ptr_size);
        multiset_delete_balance(me, traverse_parent, 0);
    }
}

/*
 * Removes traverse when it has one child.
 */
static void multiset_remove_one_child(multiset me, const char *const traverse)
{
    char *traverse_parent;
    char *traverse_left;
    char *traverse_right;
    char *traverse_parent_left;
    memcpy(&traverse_parent, traverse + node_parent_offset, ptr_size);
    memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
    memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    /* If no parent, make the child of traverse the new root. */
    if (!traverse_parent) {
        if (traverse_left) {
            memset(traverse_left + node_parent_offset, 0, ptr_size);
            me->root = traverse_left;
        } else {
            memset(traverse_right + node_parent_offset, 0, ptr_size);
            me->root = traverse_right;
        }
        return;
    }
    memcpy(&traverse_parent_left, traverse_parent + node_left_child_offset,
           ptr_size);
    /* The parent of traverse now references the child of traverse. */
    if (traverse_parent_left == traverse) {
        if (traverse_left) {
            memcpy(traverse_parent + node_left_child_offset, &traverse_left,
                   ptr_size);
            memcpy(traverse_left + node_parent_offset, &traverse_parent,
                   ptr_size);
        } else {
            memcpy(traverse_parent + node_left_child_offset, &traverse_right,
                   ptr_size);
            memcpy(traverse_right + node_parent_offset, &traverse_parent,
                   ptr_size);
        }
        multiset_delete_balance(me, traverse_parent, 1);
    } else {
        if (traverse_left) {
            memcpy(traverse_parent + node_right_child_offset, &traverse_left,
                   ptr_size);
            memcpy(traverse_left + node_parent_offset, &traverse_parent,
                   ptr_size);
        } else {
            memcpy(traverse_parent + node_right_child_offset, &traverse_right,
                   ptr_size);
            memcpy(traverse_right + node_parent_offset, &traverse_parent,
                   ptr_size);
        }
        multiset_delete_balance(me, traverse_parent, 0);
    }
}

/*
 * Removes traverse when it has two children.
 */
static void multiset_remove_two_children(multiset me,
                                         const char *const traverse)
{
    char *item;
    char *item_parent;
    char *parent;
    char *traverse_parent;
    char *traverse_right;
    char *traverse_right_left;
    int is_left_deleted;
    memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    memcpy(&traverse_right_left, traverse_right + node_left_child_offset,
           ptr_size);
    is_left_deleted = traverse_right_left != NULL;
    if (!is_left_deleted) {
        char *item_left;
        memcpy(&item, traverse + node_right_child_offset, ptr_size);
        parent = item;
        item[0] = traverse[0];
        memcpy(item + node_parent_offset, traverse + node_parent_offset,
               ptr_size);
        memcpy(item + node_left_child_offset, traverse + node_left_child_offset,
               ptr_size);
        memcpy(&item_left, item + node_left_child_offset, ptr_size);
        memcpy(item_left + node_parent_offset, &item, ptr_size);
    } else {
        char *item_left;
        char *item_right;
        item = traverse_right_left;
        memcpy(&item_left, item + node_left_child_offset, ptr_size);
        while (item_left) {
            item = item_left;
            memcpy(&item_left, item + node_left_child_offset, ptr_size);
        }
        memcpy(&parent, item + node_parent_offset, ptr_size);
        item[0] = traverse[0];
        memcpy(&item_parent, item + node_parent_offset, ptr_size);
        memcpy(item_parent + node_left_child_offset,
               item + node_right_child_offset, ptr_size);
        memcpy(&item_right, item + node_right_child_offset, ptr_size);
        if (item_right) {
            memcpy(item_right + node_parent_offset, item + node_parent_offset,
                   ptr_size);
        }
        memcpy(item + node_left_child_offset, traverse + node_left_child_offset,
               ptr_size);
        memcpy(&item_left, item + node_left_child_offset, ptr_size);
        memcpy(item_left + node_parent_offset, &item, ptr_size);
        memcpy(item + node_right_child_offset,
               traverse + node_right_child_offset, ptr_size);
        memcpy(&item_right, item + node_right_child_offset, ptr_size);
        memcpy(item_right + node_parent_offset, &item, ptr_size);
        memcpy(item + node_parent_offset, traverse + node_parent_offset,
               ptr_size);
    }
    memcpy(&traverse_parent, traverse + node_parent_offset, ptr_size);
    if (!traverse_parent) {
        me->root = item;
    } else {
        char *traverse_parent_left;
        memcpy(&traverse_parent_left, traverse_parent + node_left_child_offset,
               ptr_size);
        memcpy(&item_parent, item + node_parent_offset, ptr_size);
        if (traverse_parent_left == traverse) {
            memcpy(item_parent + node_left_child_offset, &item, ptr_size);
        } else {
            memcpy(item_parent + node_right_child_offset, &item, ptr_size);
        }
    }
    multiset_delete_balance(me, parent, is_left_deleted);
}

/*
 * Removes the element from the set.
 */
static void multiset_remove_element(multiset me, char *const traverse)
{
    char *traverse_left;
    char *traverse_right;
    memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
    memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    if (!traverse_left && !traverse_right) {
        multiset_remove_no_children(me, traverse);
    } else if (!traverse_left || !traverse_right) {
        multiset_remove_one_child(me, traverse);
    } else {
        multiset_remove_two_children(me, traverse);
    }
    free(traverse);
}

/**
 * Removes a key from the multi-set if it contains it. The pointer to the key
 * being passed in should point to the key type which this multi-set holds. For
 * example, if this multi-set holds key integers, the key pointer should be a
 * pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the multi-set to remove a key from
 * @param key the key to remove
 *
 * @return BK_TRUE if the multi-set contained the key, otherwise BK_FALSE
 */
bk_bool multiset_remove(multiset me, void *const key)
{
    char *const traverse = multiset_equal_match(me, key);
    size_t traverse_count;
    if (!traverse) {
        return BK_FALSE;
    }
    memcpy(&traverse_count, traverse + node_count_offset, count_size);
    if (traverse_count == 1) {
        multiset_remove_element(me, traverse);
    } else {
        traverse_count--;
        memcpy(traverse + node_count_offset, &traverse_count, count_size);
    }
    me->size--;
    return BK_TRUE;
}

/**
 * Removes all the occurrences of a specified key in the multi-set. The pointer
 * to the key being passed in should point to the key type which this multi-set
 * holds. For example, if this multi-set holds key integers, the key pointer
 * should be a pointer to an integer. Since the key is being copied, the pointer
 * only has to be valid when this function is called.
 *
 * @param me  the multi-set to remove a key from
 * @param key the key to remove
 *
 * @return BK_TRUE if the multi-set contained the key, otherwise BK_FALSE
 */
bk_bool multiset_remove_all(multiset me, void *const key)
{
    char *const traverse = multiset_equal_match(me, key);
    size_t traverse_count;
    if (!traverse) {
        return BK_FALSE;
    }
    memcpy(&traverse_count, traverse + node_count_offset, count_size);
    me->size -= traverse_count;
    multiset_remove_element(me, traverse);
    return BK_TRUE;
}

/**
 * Returns the first (lowest) key in this multi-set. The returned key is a
 * pointer to the internally stored key, which should not be modified. Modifying
 * it results in undefined behaviour.
 *
 * @param me the multi-set to get the key from
 *
 * @return the lowest key in this multi-set, or NULL if it is empty
 */
void *multiset_first(multiset me)
{
    char *traverse = me->root;
    char *traverse_left;
    if (!traverse) {
        return NULL;
    }
    memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
    while (traverse_left) {
        traverse = traverse_left;
        memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
    }
    return traverse + node_key_offset;
}

/**
 * Returns the last (highest) key in this multi-set. The returned key is a
 * pointer to the internally stored key, which should not be modified. Modifying
 * it results in undefined behaviour.
 *
 * @param me the multi-set to get the key from
 *
 * @return the highest key in this multi-set, or NULL if it is empty
 */
void *multiset_last(multiset me)
{
    char *traverse = me->root;
    char *traverse_right;
    if (!traverse) {
        return NULL;
    }
    memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    while (traverse_right) {
        traverse = traverse_right;
        memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    }
    return traverse + node_key_offset;
}

/**
 * Returns the key which is strictly lower than the comparison key. Meaning that
 * the highest key which is lower than the key used for comparison is returned.
 *
 * @param me  the multi-set to get the lower key from
 * @param key the key to use for comparison
 *
 * @return the key which is strictly lower, or NULL if it does not exist
 */
void *multiset_lower(multiset me, void *const key)
{
    char *ret = NULL;
    char *traverse = me->root;
    while (traverse) {
        const int compare = me->comparator(traverse + node_key_offset, key);
        if (compare < 0) {
            ret = traverse + node_key_offset;
            memcpy(&traverse, traverse + node_right_child_offset, ptr_size);
        } else {
            memcpy(&traverse, traverse + node_left_child_offset, ptr_size);
        }
    }
    return ret;
}

/**
 * Returns the key which is strictly higher than the comparison key. Meaning
 * that the lowest key which is higher than the key used for comparison is
 * returned.
 *
 * @param me  the multi-set to get the higher key from
 * @param key the key to use for comparison
 *
 * @return the key which is strictly higher, or NULL if it does not exist
 */
void *multiset_higher(multiset me, void *const key)
{
    char *ret = NULL;
    char *traverse = me->root;
    while (traverse) {
        const int compare = me->comparator(traverse + node_key_offset, key);
        if (compare > 0) {
            ret = traverse + node_key_offset;
            memcpy(&traverse, traverse + node_left_child_offset, ptr_size);
        } else {
            memcpy(&traverse, traverse + node_right_child_offset, ptr_size);
        }
    }
    return ret;
}

/**
 * Returns the key which is the floor of the comparison key. Meaning that the
 * the highest key which is lower or equal to the key used for comparison is
 * returned.
 *
 * @param me  the multi-set to get the floor key from
 * @param key the key to use for comparison
 *
 * @return the key which is the floor, or NULL if it does not exist
 */
void *multiset_floor(multiset me, void *const key)
{
    char *ret = NULL;
    char *traverse = me->root;
    while (traverse) {
        const int compare = me->comparator(traverse + node_key_offset, key);
        if (compare <= 0) {
            ret = traverse + node_key_offset;
            memcpy(&traverse, traverse + node_right_child_offset, ptr_size);
        } else {
            memcpy(&traverse, traverse + node_left_child_offset, ptr_size);
        }
    }
    return ret;
}

/**
 * Returns the key which is the ceiling of the comparison key. Meaning that the
 * the lowest key which is higher or equal to the key used for comparison is
 * returned.
 *
 * @param me  the multi-set to get the ceiling key from
 * @param key the key to use for comparison
 *
 * @return the key which is the ceiling, or NULL if it does not exist
 */
void *multiset_ceiling(multiset me, void *const key)
{
    char *ret = NULL;
    char *traverse = me->root;
    while (traverse) {
        const int compare = me->comparator(traverse + node_key_offset, key);
        if (compare >= 0) {
            ret = traverse + node_key_offset;
            memcpy(&traverse, traverse + node_left_child_offset, ptr_size);
        } else {
            memcpy(&traverse, traverse + node_right_child_offset, ptr_size);
        }
    }
    return ret;
}

/**
 * Clears the keys from the multiset.
 *
 * @param me the multi-set to clear
 */
void multiset_clear(multiset me)
{
    while (me->root) {
        multiset_remove_element(me, me->root);
    }
    me->size = 0;
}

/**
 * Frees the multi-set memory. Performing further operations after calling this
 * function results in undefined behavior. Freeing NULL is legal, and causes no
 * operation to be performed.
 *
 * @param me the multi-set to free from memory
 *
 * @return NULL
 */
multiset multiset_destroy(multiset me)
{
    if (me) {
        multiset_clear(me);
        free(me);
    }
    return NULL;
}
