/*
 * ipc.h - Inter-Process Communication with string messages
 * 
 * Implements fast string-based message passing between Gecko and Dolphin
 * for inter-component communication.
 */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>
#include "../common/list.h"

/* message types */
#define IPC_MESSAGE_DATA   0x01
#define IPC_MESSAGE_SYSTEM 0x02
#define IPC_MESSAGE_TERMINAL 0x03
#define IPC_MESSAGE_SERVICE  0x04

/* message flags */
#define IPC_BLOCKING    0x01
#define IPC_NONBLOCKING 0x02
#define IPC_URGENT      0x04

/* message queue structure */
typedef struct message_queue {
    list_t message_list;
    void* owner;
    uint32_t max_messages;
    uint32_t current_messages;
} message_queue_t;

/* message structure */
typedef struct ipc_message {
    char message_data[1024];
    uint32_t message_length;
    uint32_t message_type;
    uint32_t message_flags;
    void* sender;
    void* receiver;
    uint64_t timestamp;
    list_node_t queue_link;
} ipc_message_t;

/* service registration */
typedef struct service_entry {
    char service_name[64];
    void* service_handler;
    message_queue_t* service_queue;
    list_node_t service_link;
} service_entry_t;

/* global structures */
extern message_queue_t system_message_queue;
extern list_t registered_services;

/* ipc initialization */
void ipc_init(void);

/* message queue management */
int ipc_create_queue(void* owner, uint32_t max_messages);
void ipc_destroy_queue(message_queue_t* queue);

/* message sending and receiving */
int ipc_send_message(void* destination, const char* message, uint32_t length, 
                    uint32_t message_type, uint32_t flags);
int ipc_receive_message(void* source, char* buffer, uint32_t* length, 
                       uint32_t* message_type, uint32_t timeout_ms);

/* service registration */
int ipc_register_service(const char* service_name, void* service_handler);
void* ipc_lookup_service(const char* service_name);
int ipc_unregister_service(const char* service_name);

/* broadcast messaging */
int ipc_broadcast_message(const char* message, uint32_t length, uint32_t message_type);

/* utility functions */
void ipc_process_messages(void);
uint32_t ipc_get_queue_size(message_queue_t* queue);

#endif /* IPC_H */