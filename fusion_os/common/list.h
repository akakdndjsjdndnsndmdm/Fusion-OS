/*
 * list.h - Simple linked list implementation
 * 
 * Basic linked list data structures for use throughout the Fusion OS.
 */

#ifndef LIST_H
#define LIST_H

#include <stdint.h>

/* basic doubly-linked list node */
typedef struct list_node {
    struct list_node* next;
    struct list_node* prev;
    void* data;
} list_node_t;

/* list structure */
typedef struct {
    list_node_t* head;
    list_node_t* tail;
    uint32_t count;
} list_t;

/* initialize a list */
void list_init(list_t* list);

/* add node to end of list */
void list_add_tail(list_t* list, list_node_t* node);

/* add node to beginning of list */
void list_add_head(list_t* list, list_node_t* node);

/* remove node from list */
void list_remove(list_t* list, list_node_t* node);

/* get first node in list */
list_node_t* list_get_head(list_t* list);

/* get last node in list */
list_node_t* list_get_tail(list_t* list);

/* check if list is empty */
int list_is_empty(list_t* list);

/* get list count */
uint32_t list_count(list_t* list);

/* create new list node */
list_node_t* list_create_node(void* data);

/* destroy list node */
void list_destroy_node(list_node_t* node);

#endif /* LIST_H */