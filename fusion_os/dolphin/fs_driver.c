#include "fs_driver.h"
#include "../common/vfs.h"
#include "../common/logger.h"
#include "../common/string.h"
#include "../gecko/scheduler.h"
#include "../gecko/gecko.h"
#include <stdarg.h>

static int fs_driver_initialized = 0;
static char* memory_filesystem = NULL;
static size_t memory_filesystem_size = 0x100000;

typedef struct {
    char path[FS_MAX_PATH_LENGTH];
    uint32_t type;
    uint32_t size;
    uint32_t permissions;
    uint32_t created;
} fs_file_entry_t;

static fs_file_entry_t* file_entries = NULL;
static size_t max_entries = 1024;
static size_t entry_count = 0;

int fs_driver_init(void) {
    if (fs_driver_initialized) {
        return 0;
    }
    
    LOG_INFO("fs_driver", "initializing file system driver");
    
    if (vfs_init() != 0) {
        LOG_ERROR("fs_driver", "failed to initialize VFS");
        return -1;
    }
    
    memory_filesystem = gecko_alloc_kernel_memory(memory_filesystem_size);
    if (!memory_filesystem) {
        LOG_ERROR("fs_driver", "failed to allocate memory for filesystem");
        return -1;
    }
    
    memset(memory_filesystem, 0, memory_filesystem_size);
    
    file_entries = gecko_alloc_kernel_memory(max_entries * sizeof(fs_file_entry_t));
    if (!file_entries) {
        LOG_ERROR("fs_driver", "failed to allocate file entry table");
        gecko_free_kernel_memory(memory_filesystem);
        return -1;
    }
    
    memset(file_entries, 0, max_entries * sizeof(fs_file_entry_t));
    entry_count = 0;
    
    fs_driver_initialized = 1;
    LOG_INFO("fs_driver", "file system driver initialized successfully");
    
    return 0;
}

static int add_file_entry(const char* path, uint32_t type, uint32_t size, uint32_t permissions) {
    if (entry_count >= max_entries) {
        return -1;
    }
    
    fs_file_entry_t* entry = &file_entries[entry_count++];
    strncpy(entry->path, path, FS_MAX_PATH_LENGTH - 1);
    entry->path[FS_MAX_PATH_LENGTH - 1] = '\0';
    entry->type = type;
    entry->size = size;
    entry->permissions = permissions;
    entry->created = 0;
    
    return 0;
}

static fs_file_entry_t* find_file_entry(const char* path) {
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(file_entries[i].path, path) == 0) {
            return &file_entries[i];
        }
    }
    return NULL;
}

int fs_driver_process(fs_request_t* request, fs_response_t* response) {
    if (!request || !response) {
        return -1;
    }
    
    memset(response, 0, sizeof(fs_response_t));
    response->status = -1;
    
    switch (request->operation) {
        case FS_OP_CREATE: {
            int file_id = vfs_open(request->path, request->flags, request->file_id);
            if (file_id >= 0) {
                response->status = 0;
                response->file_id = file_id;
                
                fs_file_entry_t* entry = find_file_entry(request->path);
                if (!entry) {
                    add_file_entry(request->path, 0, 0, request->permissions);
                }
            }
            break;
        }
        
        case FS_OP_OPEN: {
            int file_id = vfs_open(request->path, request->flags, request->file_id);
            if (file_id >= 0) {
                response->status = 0;
                response->file_id = file_id;
            }
            break;
        }
        
        case FS_OP_READ: {
            size_t bytes_read = 0;
            int result = vfs_read(request->file_id, request->buffer, 
                                 request->buffer_size, &bytes_read);
            response->status = result;
            response->bytes_read = bytes_read;
            if (result == 0 && bytes_read > 0) {
                memcpy(response->result_buffer, request->buffer, bytes_read);
                response->result_buffer[bytes_read] = '\0';
            }
            break;
        }
        
        case FS_OP_WRITE: {
            size_t bytes_written = 0;
            int result = vfs_write(request->file_id, request->buffer, 
                                  request->buffer_size, &bytes_written);
            response->status = result;
            response->bytes_written = bytes_written;
            
            fs_file_entry_t* entry = find_file_entry(request->path);
            if (entry && result == 0) {
                entry->size += bytes_written;
            }
            break;
        }
        
        case FS_OP_CLOSE: {
            int result = vfs_close(request->file_id);
            response->status = result;
            break;
        }
        
        case FS_OP_MKDIR: {
            int result = vfs_mkdir(request->path, request->permissions);
            response->status = result;
            if (result == 0) {
                add_file_entry(request->path, 1, 0, request->permissions);
            }
            break;
        }
        
        case FS_OP_LIST: {
            size_t written = 0;
            response->status = fs_list_directory(request->path, response->result_buffer, 
                                                sizeof(response->result_buffer), &written);
            response->bytes_written = written;
            break;
        }
        
        case FS_OP_SEEK: {
            int result = vfs_seek(request->file_id, request->offset, request->whence);
            response->status = result;
            break;
        }
        
        default:
            response->status = -1;
            break;
    }
    
    return 0;
}

int fs_driver_handle_request(const char* request_data, size_t request_size,
                            char* response_data, size_t* response_size) {
    if (!request_data || !response_data || !response_size) {
        return -1;
    }
    
    fs_request_t request;
    fs_response_t response;
    
    if (request_size < sizeof(fs_request_t)) {
        return -1;
    }
    
    memcpy(&request, request_data, sizeof(fs_request_t));
    
    int result = fs_driver_process(&request, &response);
    
    size_t response_size_needed = sizeof(fs_response_t);
    if (*response_size < response_size_needed) {
        return -1;
    }
    
    memcpy(response_data, &response, response_size_needed);
    *response_size = response_size_needed;
    
    return result;
}

int fs_create_file(const char* path, const char* content, size_t content_size) {
    if (!path) {
        return -1;
    }
    
    int file_id = vfs_open(path, 0, 0);
    if (file_id < 0) {
        return -1;
    }
    
    if (content && content_size > 0) {
        size_t bytes_written = 0;
        vfs_write(file_id, content, content_size, &bytes_written);
    }
    
    vfs_close(file_id);
    
    add_file_entry(path, 0, content_size, 0644);
    
    return 0;
}

int fs_read_file(const char* path, char* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!path || !buffer) {
        return -1;
    }
    
    int file_id = vfs_open(path, 0, 0);
    if (file_id < 0) {
        return -1;
    }
    
    int result = vfs_read(file_id, buffer, buffer_size, bytes_read);
    vfs_close(file_id);
    
    return result;
}

int fs_write_file(const char* path, const char* buffer, size_t buffer_size, size_t* bytes_written) {
    if (!path || !buffer) {
        return -1;
    }
    
    int file_id = vfs_open(path, 2, 0);
    if (file_id < 0) {
        return -1;
    }
    
    int result = vfs_write(file_id, buffer, buffer_size, bytes_written);
    vfs_close(file_id);
    
    return result;
}

int fs_list_directory(const char* path, char* output, size_t output_size, size_t* bytes_written) {
    if (!path || !output || !bytes_written) {
        return -1;
    }
    
    *bytes_written = 0;
    
    char temp_buffer[256];
    int written = snprintf(temp_buffer, sizeof(temp_buffer), "Directory listing for %s:\n", path);
    
    if (written >= (int)output_size) {
        return -1;
    }
    
    memcpy(output, temp_buffer, written);
    *bytes_written = written;
    
    for (size_t i = 0; i < entry_count; i++) {
        char* slash_pos = strrchr(file_entries[i].path, '/');
        const char* name = slash_pos ? (slash_pos + 1) : file_entries[i].path;
        
        int line_written = snprintf(temp_buffer, sizeof(temp_buffer), "  %s\n", name);
        
        if (*bytes_written + line_written >= output_size) {
            break;
        }
        
        memcpy(output + *bytes_written, temp_buffer, line_written);
        *bytes_written += line_written;
    }
    
    return 0;
}

int fs_mkdir(const char* path) {
    if (!path) {
        return -1;
    }
    
    int result = vfs_mkdir(path, 0755);
    if (result == 0) {
        add_file_entry(path, 1, 0, 0755);
    }
    
    return result;
}

int fs_remove_file(const char* path) {
    if (!path) {
        return -1;
    }
    
    vfs_unlink(path);
    
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(file_entries[i].path, path) == 0) {
            file_entries[i] = file_entries[entry_count - 1];
            entry_count--;
            break;
        }
    }
    
    return 0;
}

int fs_remove_directory(const char* path) {
    if (!path) {
        return -1;
    }
    
    int result = vfs_rmdir(path);
    if (result == 0) {
        for (size_t i = 0; i < entry_count; i++) {
            if (strcmp(file_entries[i].path, path) == 0) {
                file_entries[i] = file_entries[entry_count - 1];
                entry_count--;
                break;
            }
        }
    }
    
    return result;
}

int fs_get_file_info(const char* path, uint32_t* size, uint32_t* type, uint32_t* permissions) {
    if (!path) {
        return -1;
    }
    
    fs_file_entry_t* entry = find_file_entry(path);
    if (!entry) {
        return -1;
    }
    
    if (size) {
        *size = entry->size;
    }
    if (type) {
        *type = entry->type;
    }
    if (permissions) {
        *permissions = entry->permissions;
    }
    
    return 0;
}