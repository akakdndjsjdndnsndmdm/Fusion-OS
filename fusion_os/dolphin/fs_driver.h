#ifndef FS_DRIVER_H
#define FS_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#define FS_DRIVER_SERVICE_NAME "fs_driver"
#define FS_MAX_BUFFER_SIZE 4096
#define FS_MAX_PATH_LENGTH 256
#define FS_MAX_FILENAME_LENGTH 64

typedef enum {
    FS_OP_OPEN = 1,
    FS_OP_READ = 2,
    FS_OP_WRITE = 3,
    FS_OP_CLOSE = 4,
    FS_OP_MKDIR = 5,
    FS_OP_RMDIR = 6,
    FS_OP_UNLINK = 7,
    FS_OP_STAT = 8,
    FS_OP_LIST = 9,
    FS_OP_SEEK = 10,
    FS_OP_CREATE = 11
} fs_operation_t;

typedef struct {
    fs_operation_t operation;
    uint32_t file_id;
    uint32_t flags;
    uint32_t permissions;
    size_t buffer_size;
    uint32_t offset;
    uint32_t whence;
    char path[FS_MAX_PATH_LENGTH];
    char buffer[FS_MAX_BUFFER_SIZE];
} fs_request_t;

typedef struct {
    int32_t status;
    uint32_t file_id;
    size_t bytes_read;
    size_t bytes_written;
    size_t file_size;
    uint32_t file_type;
    uint32_t permissions;
    char result_buffer[FS_MAX_BUFFER_SIZE];
} fs_response_t;

int fs_driver_init(void);
int fs_driver_handle_request(const char* request, size_t request_size, 
                            char* response, size_t* response_size);
int fs_driver_process(fs_request_t* request, fs_response_t* response);

int fs_create_file(const char* path, const char* content, size_t content_size);
int fs_read_file(const char* path, char* buffer, size_t buffer_size, size_t* bytes_read);
int fs_write_file(const char* path, const char* buffer, size_t buffer_size, size_t* bytes_written);
int fs_list_directory(const char* path, char* output, size_t output_size, size_t* bytes_written);
int fs_mkdir(const char* path);
int fs_remove_file(const char* path);
int fs_remove_directory(const char* path);

int fs_get_file_info(const char* path, uint32_t* size, uint32_t* type, uint32_t* permissions);

#endif