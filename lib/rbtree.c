#include <string.h>
#include "rbtree.h"
#include "memory.h"

RedBlackTree * STDCALL rbtree_init(void)
{
    RedBlackTree *root = (RedBlackTree *) SF_CALLOC(1, sizeof(RedBlackNode));

    if (!root)
    {
        log_error("rbtree_init : failed to allocate memory\n");
        return NULL;
    }

    root->key = NULL;
    root->elem = NULL;
    root->left = NULL;
    root->right = NULL;
    root->parent = NULL;
    root->color = BLACK;

    return root;
}

RedBlackNode * STDCALL rbtree_new_node(void)
{
    return ((RedBlackNode *)SF_CALLOC(1,sizeof(RedBlackNode)));
}

int STDCALL rbtree_is_left_child(RedBlackNode *node)
{
    if (!node->parent || node == node->parent->left)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

RedBlackNode * STDCALL rbtree_get_uncle(RedBlackTree *target)
{
    if (!target->parent->parent)
    {
        goto done;
    }

    if (target->parent == target->parent->parent->right)
    {
        return target->parent->parent->left;
    }
    else
    {
        return target->parent->parent->right;
    }

done:
    return NULL;
}

void STDCALL rbtree_set_color(RedBlackNode *node, Color color)
{
    if (node)
    {
        node->color = color;
    }
}

Color STDCALL rbtree_get_color(RedBlackNode *node)
{
    if (!node)
    {
        return BLACK;
    }
    
    return node->color;
}

void STDCALL rbtree_rotate_left(RedBlackTree **tree, RedBlackNode *node)
{
    RedBlackNode *rchild = node->right;
    RedBlackNode *parent = node->parent;

    rchild->parent = parent;
    node->right = rchild->left;
    if (rchild->left)
    {
        rchild->left->parent = node;
    }
    rchild->left = node;
    node->parent = rchild;
    if (!parent)
    {
        *tree = rchild;
    }
    else if (node == parent->right)
    {
        parent->right = rchild;
    }
    else
    {
        parent->left = rchild;
    }
}

void STDCALL rbtree_rotate_right(RedBlackTree **tree, RedBlackNode *node) {
    RedBlackNode *lchild = node->left;
    RedBlackNode *parent = node->parent;

    lchild->parent = parent;
    node->left = lchild->right;
    if (lchild->right)
    {
        lchild->right->parent = node;
    }
    lchild->right = node;
    node->parent = lchild;
    if (!parent)
    {
        *tree = lchild;
    }
    else if (node == parent->right)
    {
        parent->right = lchild;
    }
    else
    {
        parent->left = lchild;
    }
}

SF_INT_RET_CODE STDCALL rbtree_fix_tree(RedBlackTree **tree, RedBlackNode *target) {
    RedBlackNode *parent = NULL;
    RedBlackNode *temp_node = NULL;
    RedBlackNode *uncle = NULL;

    if (!target)
    {
        log_debug("rbtree_Fix_tree: tree passed is NULL\n");
        return SF_INT_RET_CODE_ERROR;
    }
    while((target != *tree) && (rbtree_get_color(target->parent) == RED))
    {
        /* if target's parent is a left child */
        if (target->parent == target->parent->parent->left)
        {
            uncle = rbtree_get_uncle(target);
            if (rbtree_get_color(uncle) == RED)
            {
                /* Flip Colors */
                rbtree_set_color(target->parent, BLACK);
                rbtree_set_color(uncle, BLACK);
                rbtree_set_color(target->parent->parent, RED);
                target = target->parent->parent;
                continue;
            }
            else
            {
                if (target == target->parent->right)
                {
                    target = target->parent;
                    rbtree_rotate_left(tree, target);
                }
                rbtree_set_color(target->parent, BLACK);
                rbtree_set_color(target->parent->parent, RED);
                rbtree_rotate_right(tree, target->parent->parent);
            }
        }
        else
        {
            uncle = rbtree_get_uncle(target);
            if (rbtree_get_color(uncle) == RED)
            {
                /* Flip Color */
                rbtree_set_color(target->parent, BLACK);
                rbtree_set_color(uncle, BLACK);
                rbtree_set_color(target->parent->parent, RED);
                target = target->parent->parent;
                continue;
            }
            else
            {
                if (target == target->parent->left)
                {
                    target = target->parent;
                    rbtree_rotate_right(tree, target);
                }
                rbtree_set_color(target->parent, BLACK);
                rbtree_set_color(target->parent->parent, RED);
                rbtree_rotate_left(tree, target->parent->parent);
            }
        }
    }
    (*tree)->color = BLACK;
    return SF_INT_RET_CODE_SUCCESS;
}
SF_INT_RET_CODE STDCALL rbtree_insert(RedBlackTree **T, void *param, char *name)
{
    int cmp = 0;
    RedBlackNode * node;
    SF_INT_RET_CODE retval = SF_INT_RET_CODE_ERROR;

    /*
     * Tree cannot be NULL
     * Cannot insert NULL params
     * Name cannot be NULL as it is key
     * to node search.
     */
    if (!T || !(*T) || !param || !name)
    {
        return SF_INT_RET_CODE_ERROR;
    }
    node = *T;

    if (!node->elem)
    {
        node->elem = param;
        node->key = name;
        retval = SF_INT_RET_CODE_SUCCESS;
        goto done;
    }
    while (1)
    {
        cmp = strcmp(name, node->key);
        if (cmp == 0)
        {
            /* Key Exists */
            /* Update*/
            log_debug("rbtree_insert: Duplicate param found, Overwrite\n");
            node->key = name;
            node->elem = param;
            retval = SF_INT_RET_CODE_DUPLICATES;
            goto done;
        }

        else if (cmp > 0)
        {
            if (node->right != NULL)
            {
                node = node->right;
            }
            else
            {
                node->right = rbtree_new_node();
                if (!node->right)
                {
                    retval = SF_INT_RET_CODE_ERROR;
                    log_error("rbtree_insert : Not able to allocate new rbtree node \n");
                    goto done;
                }
                node->right->key = name;
                node->right->elem = param;
                node->right->parent = node;
                node->right->color = RED;
                rbtree_fix_tree(T, node->right);
                retval = SF_INT_RET_CODE_SUCCESS;
                goto done;
            }
         }

         else
         {
            if (node->left != NULL)
            {
                node = node->left;
            }
            else
            {
                node->left = rbtree_new_node();
                if (!node->left)
                {
                    retval = SF_INT_RET_CODE_ERROR;
                    goto done;
                }
                node->left->key = name;
                node->left->elem = param;
                node->left->parent = node;
                node->left->color = RED;
                rbtree_fix_tree(T, node->left);
                retval = SF_INT_RET_CODE_SUCCESS;
                goto done;
            }
         }
     }
    done:
     return retval;
 }

void * STDCALL rbtree_search_node(RedBlackTree *tree, char *key)
{
    RedBlackNode *node = tree;
    int cmp = 0;

    if (!tree || !tree->elem || !key)
    {
        return NULL;
    }
    while(1)
    {
        cmp = strcmp(key, node->key);
        if (cmp == 0)
        {
            return node->elem;
        }
        else if(cmp < 0)
        {
            node = node->left;
            if (!node)
            {
                return NULL;
            }
        }
        else
        {
            node = node->right;
            if (!node)
            {
                return NULL;
            }
        }
     }
 }

void STDCALL rbtree_deallocate(RedBlackNode *node)
{
    if (!node)
    {
        return;
    }

    rbtree_deallocate(node->left);
    rbtree_deallocate(node->right);
    node->left = NULL;
    node->right = NULL;
    SF_FREE(node);
}
