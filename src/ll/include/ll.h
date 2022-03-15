// ref: http://www.cs.columbia.edu/~aya/W3101-01/lectures/linked-list.pdf

#ifndef _LIST_H
#define _LIST_H

#include <stdlib.h>

/**
 * @brief typedef for a custom free function for a node in this list
 * allows for heap alloc'd data in node->data to be freed correctly
 *
 * @param p - will always be a pointer to the node to be freed
 *
 */
typedef void (*node_free)(void *p);

/**
 * @brief node for the linked list
 *
 * @param data - pointer to data that the node contains; can be
 *        nested structs as long as f frees it appropriately
 *
 * @param next - pointer to next node in list
 *
 * @param f - pointer to function to free node in the list; see
 *        node_free aboce
 *
 */
typedef struct _node node;
struct _node
{
    void *    data;
    node *    next;
    node_free f;
};

/**
 * @brief linked list
 *
 * @param head - pointer to head of the list
 *
 */
typedef struct _ll
{
    node *head;
} ll;

/**
 * @brief intialized the linked list
 *
 * @param head - pointer to head of the list
 *
 * @return pointer to initialized linked list; NULL on error
 *
 */
ll *ll_init(void);

/**
 * @brief inserts a node into the list
 *
 * @param list - pointer to linked list
 *
 * @param i - index to insert node
 *
 * @param data - pointer to data to be stored in node
 *
 * @param f - free function for the node
 *
 * @return 0 on success; nonzero on error
 *
 */
int ll_insert(ll *list, uint i, void *data, node_free f);

/**
 * @brief returns length of the list
 *
 * @param list - pointer to linked list
 *
 * @return length of the list; -1 on error
 *
 */
int ll_len(ll *list);

/**
 * @brief frees all resources used by the linked list
 *
 * @param list - pointer to linked list
 *
 * @return 0 on success; nonzero on error
 *
 */
int ll_destroy(ll *list);

/**
 * @brief changes a node's data
 *
 * @param list - pointer to linked list
 *
 * @param i - index of node to change
 *
 * @param data - data to replace node->data with
 *
 * @return 0 on success; nonzero on error
 *
 */
int ll_set(ll *list, uint i, void *data);

/**
 * @brief gives a pointer to a node at index i
 *
 * @param list - pointer to linked list
 *
 * @param i - index of node to get
 *
 * @return point to node at i; NULL on error
 *
 */
node *ll_get(ll *list, uint i);

/**
 * @brief removes node at index i
 *
 * @param list - pointer to linked list
 *
 * @param i - index of node to remove
 *
 * @return 0 on success; nonzero on error
 *
 */
int ll_rm(ll *, uint i);

/**
 * @brief prints ascii form of the list to stdout
 *
 * @param list - pointer to linked list
 *
 * @return nothing
 *
 */
void ll_print(ll *list);

/**
 * @brief inserts node to front of list
 *
 * @param list - pointer to linked list
 *
 * @param data = pointer to data for node
 *
 * @param f - free function for the node
 *
 * @return 0 on success; nonzero on error
 *
 */
int push_front(ll *list, void *data, node_free f);

/**
 * @brief inserts node to back of list
 *
 * @param list - pointer to linked list
 *
 * @param data = pointer to data for node
 *
 * @param f - free function for the node
 *
 * @return 0 on success; nonzero on error
 *
 */
int push_back(ll *list, void *data, node_free f);

/**
 * @brief removes node from front of list
 *        NOTE: this function frees the node but
 *        not the data it is up to the caller to
 *        free the data
 *
 * @param list - pointer to linked list
 *
 * @return pointer to data of removed node
 *
 */
void *pop_front(ll *list);

/**
 * @brief removes node from back of list
 *        NOTE: this function frees the node but
 *        not the data it is up to the caller to
 *        free the data
 *
 * @param list - pointer to linked list
 *
 * @return pointer to data of removed node
 *
 */
void *pop_back(ll *list);

#endif /* _LIST_H */
