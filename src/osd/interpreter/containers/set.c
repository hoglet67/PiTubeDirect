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
#include "set.h"

struct internal_set {
    size_t size;
    size_t key_size;
    int (*comparator)(const void *const one, const void *const two);
    char *root;
};

static const size_t ptr_size = sizeof(char *);
/* Node balance is always the first byte (at index 0). */
static const size_t node_parent_offset = sizeof(signed char);
static const size_t node_left_child_offset = 1 + sizeof(char *);
static const size_t node_right_child_offset = 1 + 2 * sizeof(char *);
static const size_t node_key_offset = 1 + 3 * sizeof(char *);

/**
 * Initializes a set.
 *
 * @param key_size   the size of each element in the set; must be positive
 * @param comparator the comparator function used for key ordering; must not be
 *                   NULL
 *
 * @return the newly-initialized set, or NULL if it was not successfully
 *         initialized due to either invalid input arguments or memory
 *         allocation error
 */
set set_init(const size_t key_size,
             int (*const comparator)(const void *const, const void *const))
{
    struct internal_set *init;
    if (key_size == 0 || !comparator) {
        return NULL;
    }
    if (node_key_offset + key_size < node_key_offset) {
        return NULL;
    }
    init = malloc(sizeof(struct internal_set));
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
 * Gets the size of the set.
 *
 * @param me the set to check
 *
 * @return the size of the set
 */
size_t set_size(set me)
{
    return me->size;
}

/**
 * Determines whether or not the set is empty.
 *
 * @param me the set to check
 *
 * @return BK_TRUE if the set is empty, otherwise BK_FALSE
 */
bk_bool set_is_empty(set me)
{
    return set_size(me) == 0;
}

/*
 * Resets the parent reference.
 */
static void set_reference_parent(set me, char *const parent, char *const child)
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
static void set_rotate_left(set me, char *const parent, char *const child)
{
    char *left_grand_child;
    set_reference_parent(me, parent, child);
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
static void set_rotate_right(set me, char *const parent, char *const child)
{
    char *right_grand_child;
    set_reference_parent(me, parent, child);
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
static char *set_repair_left(set me, char *const parent, char *const child)
{
    set_rotate_left(me, parent, child);
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
static char *set_repair_right(set me, char *const parent, char *const child)
{
    set_rotate_right(me, parent, child);
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
static char *set_repair_left_right(set me, char *const parent,
                                   char *const child, char *const grand_child)
{
    set_rotate_left(me, child, grand_child);
    set_rotate_right(me, parent, grand_child);
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
static char *set_repair_right_left(set me, char *const parent,
                                   char *const child, char *const grand_child)
{
    set_rotate_right(me, child, grand_child);
    set_rotate_left(me, parent, grand_child);
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
static char *set_repair(set me, char *const parent, char *const child,
                        char *const grand_child)
{
    if (parent[0] == 2) {
        if (child[0] == -1) {
            return set_repair_right_left(me, parent, child, grand_child);
        }
        return set_repair_left(me, parent, child);
    }
    if (child[0] == 1) {
        return set_repair_left_right(me, parent, child, grand_child);
    }
    return set_repair_right(me, parent, child);
}

/*
 * Balances the AVL tree on insert.
 */
static void set_insert_balance(set me, char *const item)
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
            set_repair(me, parent, child, grand_child);
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
static char *set_create_node(set me, const void *const data, char *const parent)
{
    char *insert = malloc(1 + 3 * ptr_size + me->key_size);
    if (!insert) {
        return NULL;
    }
    insert[0] = 0;
    memcpy(insert + node_parent_offset, &parent, ptr_size);
    memset(insert + node_left_child_offset, 0, ptr_size);
    memset(insert + node_right_child_offset, 0, ptr_size);
    memcpy(insert + node_key_offset, data, me->key_size);
    me->size++;
    return insert;
}

/**
 * Adds a key to the set if the set does not already contain it. The pointer to
 * the key being passed in should point to the key type which this set holds.
 * For example, if this set holds key integers, the key pointer should be a
 * pointer to an integer. Since the key is being copied, the pointer only has
 * to be valid when this function is called.
 *
 * @param me  the set to add to
 * @param key the key to add
 *
 * @return  BK_OK     if no error
 * @return -BK_ENOMEM if out of memory
 */
bk_err set_put(set me, void *const key)
{
    char *traverse;
    if (!me->root) {
        char *insert = set_create_node(me, key, NULL);
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
                char *insert = set_create_node(me, key, traverse);
                if (!insert) {
                    return -BK_ENOMEM;
                }
                memcpy(traverse + node_left_child_offset, &insert, ptr_size);
                set_insert_balance(me, insert);
                return BK_OK;
            }
        } else if (compare > 0) {
            char *traverse_right;
            memcpy(&traverse_right, traverse + node_right_child_offset,
                   ptr_size);
            if (traverse_right) {
                traverse = traverse_right;
            } else {
                char *insert = set_create_node(me, key, traverse);
                if (!insert) {
                    return -BK_ENOMEM;
                }
                memcpy(traverse + node_right_child_offset, &insert, ptr_size);
                set_insert_balance(me, insert);
                return BK_OK;
            }
        } else {
            return BK_OK;
        }
    }
}

/*
 * If a match occurs, returns the match. Else, returns NULL.
 */
static char *set_equal_match(set me, const void *const key)
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
 * Determines if the set contains the specified key. The pointer to the key
 * being passed in should point to the key type which this set holds. For
 * example, if this set holds key integers, the key pointer should be a pointer
 * to an integer. Since the key is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me  the set to check for the key
 * @param key the key to check
 *
 * @return BK_TRUE if the set contained the key, otherwise BK_FALSE
 */
bk_bool set_contains(set me, void *const key)
{
    return set_equal_match(me, key) != NULL;
}

/*
 * Repairs the AVL tree by pivoting on an item.
 */
static char *set_repair_pivot(set me, char *const item, const int is_left_pivot)
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
    return set_repair(me, item, child, grand_child);
}

/*
 * Goes back up the tree repairing it along the way.
 */
static void set_trace_ancestors(set me, char *item)
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
            child = set_repair_pivot(me, parent, parent_left == child);
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
static void set_delete_balance(set me, char *item, const int is_left_deleted)
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
        item = set_repair_pivot(me, item, is_left_deleted);
        memcpy(&item_parent, item + node_parent_offset, ptr_size);
        if (!item_parent || item[0] == -1 || item[0] == 1) {
            return;
        }
    }
    set_trace_ancestors(me, item);
}

/*
 * Removes traverse when it has no children.
 */
static void set_remove_no_children(set me, const char *const traverse)
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
        set_delete_balance(me, traverse_parent, 1);
    } else {
        memset(traverse_parent + node_right_child_offset, 0, ptr_size);
        set_delete_balance(me, traverse_parent, 0);
    }
}

/*
 * Removes traverse when it has one child.
 */
static void set_remove_one_child(set me, const char *const traverse)
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
        set_delete_balance(me, traverse_parent, 1);
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
        set_delete_balance(me, traverse_parent, 0);
    }
}

/*
 * Removes traverse when it has two children.
 */
static void set_remove_two_children(set me, const char *const traverse)
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
    set_delete_balance(me, parent, is_left_deleted);
}

/*
 * Removes the element from the set.
 */
static void set_remove_element(set me, char *const traverse)
{
    char *traverse_left;
    char *traverse_right;
    memcpy(&traverse_left, traverse + node_left_child_offset, ptr_size);
    memcpy(&traverse_right, traverse + node_right_child_offset, ptr_size);
    if (!traverse_left && !traverse_right) {
        set_remove_no_children(me, traverse);
    } else if (!traverse_left || !traverse_right) {
        set_remove_one_child(me, traverse);
    } else {
        set_remove_two_children(me, traverse);
    }
    free(traverse);
    me->size--;
}

/**
 * Removes the key from the set if it contains it. The pointer to the key
 * being passed in should point to the key type which this set holds. For
 * example, if this set holds key integers, the key pointer should be a pointer
 * to an integer. Since the key is being copied, the pointer only has to be
 * valid when this function is called.
 *
 * @param me  the set to remove an key from
 * @param key the key to remove
 *
 * @return BK_TRUE if the set contained the key, otherwise BK_FALSE
 */
bk_err set_remove(set me, void *const key)
{
    char *const traverse = set_equal_match(me, key);
    if (!traverse) {
        return BK_FALSE;
    }
    set_remove_element(me, traverse);
    return BK_TRUE;
}

/**
 * Returns the first (lowest) key in this set. The returned key is a pointer to
 * the internally stored key, which should not be modified. Modifying it results
 * in undefined behaviour.
 *
 * @param me the set to get the key from
 *
 * @return the lowest key in this set, or NULL if it is empty
 */
void *set_first(set me)
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
 * Returns the last (highest) key in this set. The returned key is a pointer to
 * the internally stored key, which should not be modified. Modifying it results
 * in undefined behaviour.
 *
 * @param me the set to get the key from
 *
 * @return the highest key in this set, or NULL if it is empty
 */
void *set_last(set me)
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
 * @param me  the set to get the lower key from
 * @param key the key to use for comparison
 *
 * @return the key which is strictly lower, or NULL if it does not exist
 */
void *set_lower(set me, void *const key)
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
 * @param me  the set to get the higher key from
 * @param key the key to use for comparison
 *
 * @return the key which is strictly higher, or NULL if it does not exist
 */
void *set_higher(set me, void *const key)
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
 * @param me  the set to get the floor key from
 * @param key the key to use for comparison
 *
 * @return the key which is the floor, or NULL if it does not exist
 */
void *set_floor(set me, void *const key)
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
 * @param me  the set to get the ceiling key from
 * @param key the key to use for comparison
 *
 * @return the key which is the ceiling, or NULL if it does not exist
 */
void *set_ceiling(set me, void *const key)
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
 * Clears the keys from the set.
 *
 * @param me the set to clear
 */
void set_clear(set me)
{
    while (me->root) {
        set_remove_element(me, me->root);
    }
}

/**
 * Frees the set memory. Performing further operations after calling this
 * function results in undefined behavior. Freeing NULL is legal, and causes
 * no operation to be performed.
 *
 * @param me the set to free from memory
 *
 * @return NULL
 */
set set_destroy(set me)
{
    if (me) {
        set_clear(me);
        free(me);
    }
    return NULL;
}
