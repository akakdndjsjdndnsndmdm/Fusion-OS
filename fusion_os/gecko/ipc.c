/*
 * ipc.c - Inter-Process Communication implementation
 * 
 * Implements fast string-based message passing for Fusion OS components.
 */

#include "ipc.h"
#include "scheduler.h"
#include "pmm.h"
#include "../common/logger.h"
#include "../common/string.h"

/* global ipc structures */
message_queue_t system_message_queue;
list_t registered_services;
static int ipc_initialized = 0;

/* maximum number of services */
#define MAX_SERVICES 64
static service_entry_t service_registry[MAX_SERVICES];
static uint32_t service_count = 0;

/*
 * create new message
 */
static ipc_message_t* create_message(const char* data, uint32_t length, 
                                    uint32_t type, uint32_t flags) {
    if (length > sizeof(((ipc_message_t*)0)->message_data)) {
        LOG_WARNING("ipc", "message too large: %u bytes", length);
        return NULL;
    }
    
    /* allocate message from memory pool */
    ipc_message_t* message = (ipc_message_t*)pmm_alloc_page();
    if (message == NULL) {
        LOG_ERROR("ipc", "failed to allocate message memory");
        return NULL;
    }
    
    /* initialize message */
    memcpy(message->message_data, data, length);
    message->message_data[length] = '\0'; /* null terminate for safety */
    message->message_length = length;
    message->message_type = type;
    message->message_flags = flags;
    
    /* get actual timestamp from system timer */
    extern uint64_t gecko_get_uptime(void);
    message->timestamp = gecko_get_uptime();
    
    /* initialize queue link */
    message->queue_link.data = message;
    message->queue_link.next = NULL;
    message->queue_link.prev = NULL;
    
    return message;
}

/*
 * initialize ipc system
 */
void ipc_init(void) {
    if (ipc_initialized) {
        return;
    }
    
    LOG_INFO("ipc", "initializing ipc system");
    
    /* initialize system message queue */
    list_init(&system_message_queue.message_list);
    system_message_queue.owner = NULL; /* system queue */
    system_message_queue.max_messages = 1024;
    system_message_queue.current_messages = 0;
    
    /* initialize service registry */
    list_init(&registered_services);
    memset(service_registry, 0, sizeof(service_registry));
    service_count = 0;
    
    ipc_initialized = 1;
    LOG_INFO("ipc", "ipc system initialized");
}

/*
 * create message queue
 */
int ipc_create_queue(void* owner, uint32_t max_messages) {
    if (!ipc_initialized) {
        ipc_init();
    }
    
    /* allocate message queue from memory pool */
    message_queue_t* queue = (message_queue_t*)pmm_alloc_page();
    if (queue == NULL) {
        LOG_ERROR("ipc", "failed to allocate message queue memory");
        return -1;
    }
    
    list_init(&queue->message_list);
    queue->owner = owner;
    queue->max_messages = max_messages;
    queue->current_messages = 0;
    
    LOG_INFO("ipc", "created message queue for owner %p (max: %u messages)", 
             owner, max_messages);
    
    return 0; /* success */
}

/*
 * destroy message queue
 */
void ipc_destroy_queue(message_queue_t* queue) {
    if (queue == NULL) {
        return;
    }
    
    /* clear all messages from queue */
    list_node_t* node = list_get_head(&queue->message_list);
    while (node != NULL) {
        list_node_t* next = node->next;
        ipc_message_t* message = (ipc_message_t*)node->data;
        /* free message */
        node = next;
    }
    
    LOG_INFO("ipc", "destroyed message queue for owner %p", queue->owner);
}

/*
 * send message to destination
 */
int ipc_send_message(void* destination, const char* message, uint32_t length, 
                    uint32_t message_type, uint32_t flags) {
    if (!ipc_initialized) {
        ipc_init();
    }
    
    /* validate message */
    if (message == NULL || length == 0 || length > 1024) {
        LOG_WARNING("ipc", "invalid message parameters");
        return -1;
    }
    
    /* create message */
    ipc_message_t* ipc_msg = create_message(message, length, message_type, flags);
    if (ipc_msg == NULL) {
        return -1;
    }
    
    /* set sender and receiver */
    extern task_t* scheduler_get_current_task(void);
    ipc_msg->sender = scheduler_get_current_task();
    ipc_msg->receiver = destination;
    
    /* handle system messages */
    if (destination == NULL) {
        /* broadcast to system queue */
        if (system_message_queue.current_messages < system_message_queue.max_messages) {
            list_add_tail(&system_message_queue.message_list, &ipc_msg->queue_link);
            system_message_queue.current_messages++;
            LOG_DEBUG("ipc", "sent system message: %s", message);
        } else {
            LOG_WARNING("ipc", "system message queue full");
            return -1;
        }
    } else {
        /* send to specific destination */
        message_queue_t* dest_queue = (message_queue_t*)destination;
        if (dest_queue->current_messages < dest_queue->max_messages) {
            list_add_tail(&dest_queue->message_list, &ipc_msg->queue_link);
            dest_queue->current_messages++;
            LOG_DEBUG("ipc", "sent message to %p: %s", destination, message);
        } else {
            LOG_WARNING("ipc", "destination queue full");
            return -1;
        }
    }
    
    return 0;
}

/*
 * receive message from source
 */
int ipc_receive_message(void* source, char* buffer, uint32_t* length, 
                       uint32_t* message_type, uint32_t timeout_ms) {
    if (!ipc_initialized) {
        ipc_init();
    }
    
    message_queue_t* queue = (message_queue_t*)source;
    if (queue == NULL) {
        queue = &system_message_queue; /* receive from system queue */
    }
    
    /* wait for message if queue is empty */
    uint32_t wait_time = 0;
    while (list_is_empty(&queue->message_list) && wait_time < timeout_ms) {
        /* simple busy wait - in real implementation would use proper synchronization */
        wait_time++;
    }
    
    if (list_is_empty(&queue->message_list)) {
        LOG_DEBUG("ipc", "timeout waiting for message");
        return -1;
    }
    
    /* get first message */
    list_node_t* node = list_get_head(&queue->message_list);
    ipc_message_t* message = (ipc_message_t*)node->data;
    
    /* remove from queue */
    list_remove(&queue->message_list, node);
    queue->current_messages--;
    
    /* copy message data */
    if (*length < message->message_length) {
        LOG_WARNING("ipc", "buffer too small for message");
        return -1;
    }
    
    memcpy(buffer, message->message_data, message->message_length);
    buffer[message->message_length] = '\0';
    *length = message->message_length;
    *message_type = message->message_type;
    
    LOG_DEBUG("ipc", "received message: %s", buffer);
    
    /* free message using physical memory manager */
    pmm_free_page(message);
    
    return 0;
}

/*
 * register service
 */
int ipc_register_service(const char* service_name, void* service_handler) {
    if (!ipc_initialized) {
        ipc_init();
    }
    
    if (service_count >= MAX_SERVICES) {
        LOG_WARNING("ipc", "maximum services reached");
        return -1;
    }
    
    /* check if service already exists */
    for (uint32_t i = 0; i < service_count; i++) {
        if (strcmp(service_registry[i].service_name, service_name) == 0) {
            LOG_WARNING("ipc", "service %s already registered", service_name);
            return -1;
        }
    }
    
    /* register new service */
    service_entry_t* service = &service_registry[service_count];
    strncpy(service->service_name, service_name, sizeof(service->service_name) - 1);
    service->service_handler = service_handler;
    
    /* create service queue */
    if (ipc_create_queue(service, 64) == 0) {
        /* allocate separate queue for this service */
        message_queue_t* service_queue = (message_queue_t*)pmm_alloc_page();
        if (service_queue != NULL) {
            list_init(&service_queue->message_list);
            service_queue->owner = service;
            service_queue->max_messages = 64;
            service_queue->current_messages = 0;
            service->service_queue = service_queue;
        } else {
            service->service_queue = &system_message_queue;
        }
    }
    
    service->service_link.data = service;
    list_add_tail(&registered_services, &service->service_link);
    service_count++;
    
    LOG_INFO("ipc", "registered service: %s", service_name);
    return 0;
}

/*
 * lookup service
 */
void* ipc_lookup_service(const char* service_name) {
    if (!ipc_initialized) {
        ipc_init();
    }
    
    /* search registered services */
    list_node_t* node = list_get_head(&registered_services);
    while (node != NULL) {
        service_entry_t* service = (service_entry_t*)node->data;
        if (strcmp(service->service_name, service_name) == 0) {
            return service->service_handler;
        }
        node = node->next;
    }
    
    return NULL;
}

/*
 * unregister service
 */
int ipc_unregister_service(const char* service_name) {
    /* find and remove service */
    list_node_t* node = list_get_head(&registered_services);
    while (node != NULL) {
        service_entry_t* service = (service_entry_t*)node->data;
        if (strcmp(service->service_name, service_name) == 0) {
            list_remove(&registered_services, node);
            /* destroy service queue */
            if (service->service_queue != NULL) {
                ipc_destroy_queue(service->service_queue);
            }
            service_count--;
            LOG_INFO("ipc", "unregistered service: %s", service_name);
            return 0;
        }
        node = node->next;
    }
    
    return -1;
}

/*
 * broadcast message to all services
 */
int ipc_broadcast_message(const char* message, uint32_t length, uint32_t message_type) {
    int result = 0;
    
    /* send to system queue */
    if (ipc_send_message(NULL, message, length, message_type, IPC_NONBLOCKING) == 0) {
        result++;
    }
    
    /* send to registered services */
    list_node_t* node = list_get_head(&registered_services);
    while (node != NULL) {
        service_entry_t* service = (service_entry_t*)node->data;
        if (ipc_send_message(service->service_queue, message, length, message_type, 
                           IPC_NONBLOCKING) == 0) {
            result++;
        }
        node = node->next;
    }
    
    return result;
}

/*
 * get queue size
 */
uint32_t ipc_get_queue_size(message_queue_t* queue) {
    if (queue == NULL) {
        return system_message_queue.current_messages;
    }
    return queue->current_messages;
}