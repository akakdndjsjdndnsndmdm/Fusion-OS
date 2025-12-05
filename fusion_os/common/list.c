/*
 * list.c - Simple linked list implementation
 */

#include <stddef.h>
#include "list.h"
#include "../gecko/pmm.h"

/* initialize a list */
void list_init(list_t* list) {
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

/* add node to end of list */
void list_add_tail(list_t* list, list_node_t* node) {
    if (list_is_empty(list)) {
        list->head = node;
        list->tail = node;
        node->next = NULL;
        node->prev = NULL;
    } else {
        list->tail->next = node;
        node->prev = list->tail;
        node->next = NULL;
        list->tail = node;
    }
    list->count++;
}

/* add node to beginning of list */
void list_add_head(list_t* list, list_node_t* node) {
    if (list_is_empty(list)) {
        list->head = node;
        list->tail = node;
        node->next = NULL;
        node->prev = NULL;
    } else {
        list->head->prev = node;
        node->next = list->head;
        node->prev = NULL;
        list->head = node;
    }
    list->count++;
}

/* remove node from list */
void list_remove(list_t* list, list_node_t* node) {
    if (node == NULL || list_is_empty(list)) {
        return;
    }
    
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    
    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    
    list->count--;
}

/* get first node in list */
list_node_t* list_get_head(list_t* list) {
    return list->head;
}

/* get last node in list */
list_node_t* list_get_tail(list_t* list) {
    return list->tail;
}

/* check if list is empty */
int list_is_empty(list_t* list) {
    return list->count == 0;
}

/* get list count */
uint32_t list_count(list_t* list) {
    return list->count;
}

/* create new list node */
list_node_t* list_create_node(void* data) {
    list_node_t* node = pmm_alloc(sizeof(list_node_t));
    if (node == NULL) {
        return NULL;
    }
    node->next = NULL;
    node->prev = NULL;
    node->data = data;
    return node;
}

/* destroy list node */
void list_destroy_node(list_node_t* node) {
    if (node != NULL) {
        node->next = NULL;
        node->prev = NULL;
        node->data = NULL;
        pmm_free(node, sizeof(list_node_t));
    }
}