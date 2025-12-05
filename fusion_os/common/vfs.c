#include "vfs.h"
#include "logger.h"
#include "string.h"
#include "../gecko/pmm.h"
#include "../gecko/vmm.h"
#include "../gecko/scheduler.h"
#include "../gecko/gecko.h"
#include <stdarg.h>

#define VFS_TAG 0x56465300

static int vfs_initialized = 0;
static vfs_mount_point_t mount_points[VFS_MAX_MOUNT_POINTS];
static vfs_file_t file_descriptors[VFS_MAX_FILE_DESCRIPTORS];
static vfs_inode_t* inode_cache = NULL;
static uint32_t next_inode_id = 1;
static uint32_t next_file_id = 1;

static const vfs_file_operations_t default_file_ops = {
    .open = NULL,
    .read = NULL,
    .write = NULL,
    .close = NULL,
    .seek = NULL,
    .stat = NULL,
    .unlink = NULL
};

static const vfs_inode_operations_t default_inode_ops = {
    .mkdir = NULL,
    .rmdir = NULL,
    .link = NULL,
    .unlink = NULL,
    .create_file = NULL
};

static const vfs_superblock_operations_t default_sb_ops = {
    .mount = NULL,
    .umount = NULL,
    .sync = NULL
};

int vfs_init(void) {
    if (vfs_initialized) {
        return 0;
    }
    
    LOG_INFO("vfs", "initializing virtual file system");
    
    memset(mount_points, 0, sizeof(mount_points));
    memset(file_descriptors, 0, sizeof(file_descriptors));
    
    inode_cache = NULL;
    next_inode_id = 1;
    next_file_id = 1;
    
    vfs_initialized = 1;
    LOG_INFO("vfs", "virtual file system initialized successfully");
    
    return 0;
}

int vfs_mount(const char* device, const char* mount_point, const char* fs_type) {
    if (!device || !mount_point || !fs_type || !vfs_initialized) {
        return -1;
    }
    
    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        if (!mount_points[i].active) {
            mount_points[i].active = 1;
            mount_points[i].device_name = device;
            mount_points[i].mount_point = mount_point;
            mount_points[i].path = mount_point;
            
            vfs_superblock_t* sb = gecko_alloc_kernel_memory(sizeof(vfs_superblock_t));
            if (!sb) {
                mount_points[i].active = 0;
                return -1;
            }
            
            memset(sb, 0, sizeof(vfs_superblock_t));
            sb->device_name = device;
            sb->mount_point = mount_point;
            sb->ops = &default_sb_ops;
            sb->reference_count = 1;
            
            vfs_inode_t* root_inode = gecko_alloc_kernel_memory(sizeof(vfs_inode_t));
            if (!root_inode) {
                gecko_free_kernel_memory(sb);
                mount_points[i].active = 0;
                return -1;
            }
            
            memset(root_inode, 0, sizeof(vfs_inode_t));
            root_inode->inode_id = next_inode_id++;
            root_inode->type = VFS_TYPE_DIRECTORY;
            root_inode->permissions = 0755;
            root_inode->size = 0;
            root_inode->link_count = 1;
            root_inode->ops = &default_inode_ops;
            root_inode->sb = sb;
            root_inode->reference_count = 1;
            
            sb->root_inode = root_inode;
            mount_points[i].superblock = sb;
            mount_points[i].mount_inode = root_inode;
            
            return 0;
        }
    }
    
    return -1;
}

static vfs_mount_point_t* find_mount_point(const char* path) {
    if (!path || path[0] != '/') {
        return NULL;
    }
    
    size_t path_len = strlen(path);
    vfs_mount_point_t* best_match = NULL;
    size_t best_match_len = 0;
    
    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        if (!mount_points[i].active) {
            continue;
        }
        
        const char* mount_path = mount_points[i].path;
        if (mount_path[0] != '/') {
            continue;
        }
        
        size_t mount_len = strlen(mount_path);
        
        if (path_len >= mount_len && strncmp(path, mount_path, mount_len) == 0) {
            if (mount_len > best_match_len) {
                best_match = &mount_points[i];
                best_match_len = mount_len;
            }
        }
    }
    
    return best_match;
}

vfs_inode_t* vfs_lookup(const char* path) {
    if (!path || path[0] != '/') {
        return NULL;
    }
    
    vfs_mount_point_t* mp = find_mount_point(path);
    if (!mp || !mp->superblock) {
        return NULL;
    }
    
    if (strcmp(path, "/") == 0) {
        return mp->mount_inode;
    }
    
    if (strcmp(path, mp->mount_point) == 0) {
        return mp->mount_inode;
    }
    
    const char* remaining_path = path + strlen(mp->mount_point);
    if (*remaining_path == '\0') {
        return mp->mount_inode;
    }
    
    if (*remaining_path == '/') {
        remaining_path++;
    }
    
    if (*remaining_path == '\0') {
        return mp->mount_inode;
    }
    
    if (mp->superblock->ops && mp->superblock->ops->mount) {
        char file_path[VFS_MAX_PATH_LENGTH];
        snprintf(file_path, sizeof(file_path), "%s", remaining_path);
        return NULL;
    }
    
    return mp->mount_inode;
}

int vfs_open(const char* path, uint32_t flags, uint32_t file_id) {
    (void)file_id;
    if (!path || !vfs_initialized) {
        return -1;
    }
    
    vfs_inode_t* inode = vfs_lookup(path);
    if (!inode) {
        if (flags & VFS_O_CREAT) {
            return vfs_mkdir(path, 0644);
        }
        return -1;
    }
    
    uint32_t id = next_file_id++;
    if (id >= VFS_MAX_FILE_DESCRIPTORS) {
        return -1;
    }
    
    vfs_file_t* file = &file_descriptors[id];
    file->file_id = id;
    file->inode = inode;
    file->position = 0;
    file->flags = flags;
    file->ops = &default_file_ops;
    file->private_data = NULL;
    file->reference_count = 1;
    
    if (inode->ops && inode->ops->create_file) {
        if (inode->ops->create_file(inode, path, 0644) < 0) {
            return -1;
        }
    }
    
    return id;
}

int vfs_close(uint32_t file_id) {
    if (file_id >= VFS_MAX_FILE_DESCRIPTORS) {
        return -1;
    }
    
    vfs_file_t* file = &file_descriptors[file_id];
    if (file->inode == NULL) {
        return -1;
    }
    
    if (file->ops && file->ops->close) {
        file->ops->close(file);
    }
    
    file->inode->reference_count--;
    if (file->inode->reference_count == 0) {
        if (file->inode->data) {
            gecko_free_kernel_memory(file->inode->data);
        }
        gecko_free_kernel_memory(file->inode);
    }
    
    memset(file, 0, sizeof(vfs_file_t));
    
    return 0;
}

int vfs_read(uint32_t file_id, void* buffer, size_t size, size_t* bytes_read) {
    if (file_id >= VFS_MAX_FILE_DESCRIPTORS || !buffer || !size) {
        return -1;
    }
    
    vfs_file_t* file = &file_descriptors[file_id];
    if (!file->inode) {
        return -1;
    }
    
    if (file->position >= file->inode->size) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return 0;
    }
    
    size_t to_read = size;
    if (file->position + to_read > file->inode->size) {
        to_read = file->inode->size - file->position;
    }
    
    if (file->inode->type == VFS_TYPE_FILE && file->inode->data) {
        memcpy(buffer, (char*)file->inode->data + file->position, to_read);
        file->position += to_read;
        
        if (bytes_read) {
            *bytes_read = to_read;
        }
        
        return 0;
    }
    
    if (bytes_read) {
        *bytes_read = 0;
    }
    
    return 0;
}

int vfs_write(uint32_t file_id, const void* buffer, size_t size, size_t* bytes_written) {
    if (file_id >= VFS_MAX_FILE_DESCRIPTORS || !buffer || !size) {
        return -1;
    }
    
    vfs_file_t* file = &file_descriptors[file_id];
    if (!file->inode || !(file->flags & VFS_PERM_WRITE)) {
        return -1;
    }
    
    if (file->inode->type != VFS_TYPE_FILE) {
        return -1;
    }
    
    if (!file->inode->data) {
        file->inode->data = gecko_alloc_kernel_memory(file->inode->size + size);
        if (!file->inode->data) {
            return -1;
        }
        file->inode->size = 0;
    }
    
    if (file->position + size > file->inode->size) {
        size_t new_size = file->position + size;
        void* new_data = gecko_alloc_kernel_memory(new_size);
        if (!new_data) {
            return -1;
        }
        
        memcpy(new_data, file->inode->data, file->inode->size);
        gecko_free_kernel_memory(file->inode->data);
        
        file->inode->data = new_data;
        file->inode->size = new_size;
    }
    
    memcpy((char*)file->inode->data + file->position, buffer, size);
    file->position += size;
    
    if (bytes_written) {
        *bytes_written = size;
    }
    
    return 0;
}

int vfs_mkdir(const char* path, uint32_t permissions) {
    if (!path || !vfs_initialized) {
        return -1;
    }
    
    vfs_inode_t* parent = NULL;
    const char* dirname = path;
    const char* last_slash = strrchr(path, '/');
    
    if (last_slash) {
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent = vfs_lookup("/");
        } else {
            char parent_path[VFS_MAX_PATH_LENGTH];
            memcpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            parent = vfs_lookup(parent_path);
        }
        dirname = last_slash + 1;
    }
    
    if (!parent) {
        return -1;
    }
    
    vfs_inode_t* dir_inode = gecko_alloc_kernel_memory(sizeof(vfs_inode_t));
    if (!dir_inode) {
        return -1;
    }
    
    memset(dir_inode, 0, sizeof(vfs_inode_t));
    
    dir_inode->inode_id = next_inode_id++;
    dir_inode->type = VFS_TYPE_DIRECTORY;
    dir_inode->permissions = permissions;
    dir_inode->size = 0;
    dir_inode->link_count = 1;
    dir_inode->creation_time = 0;
    dir_inode->modification_time = 0;
    dir_inode->access_time = 0;
    dir_inode->data = gecko_alloc_kernel_memory(VFS_MAX_PATH_LENGTH);
    if (dir_inode->data) {
        strncpy((char*)dir_inode->data, dirname, VFS_MAX_FILENAME_LENGTH - 1);
        ((char*)dir_inode->data)[VFS_MAX_FILENAME_LENGTH - 1] = '\0';
    }
    dir_inode->ops = &default_inode_ops;
    dir_inode->sb = NULL;
    dir_inode->reference_count = 1;
    
    return 0;
}

int vfs_umount(const char* mount_point) {
    if (!mount_point || !vfs_initialized) {
        return -1;
    }
    
    for (int i = 0; i < VFS_MAX_MOUNT_POINTS; i++) {
        if (mount_points[i].active && strcmp(mount_points[i].mount_point, mount_point) == 0) {
            if (mount_points[i].superblock->ops && mount_points[i].superblock->ops->umount) {
                mount_points[i].superblock->ops->umount(mount_points[i].superblock);
            }
            
            mount_points[i].active = 0;
            memset(&mount_points[i], 0, sizeof(vfs_mount_point_t));
            
            return 0;
        }
    }
    
    return -1;
}

vfs_superblock_t* vfs_get_superblock(const char* path) {
    vfs_mount_point_t* mp = find_mount_point(path);
    return mp ? mp->superblock : NULL;
}

int vfs_seek(uint32_t file_id, int64_t offset, int whence) {
    if (file_id >= VFS_MAX_FILE_DESCRIPTORS) {
        return -1;
    }
    
    vfs_file_t* file = &file_descriptors[file_id];
    if (!file->inode) {
        return -1;
    }
    
    int64_t new_position = file->position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = (int64_t)file->position + offset;
            break;
        case SEEK_END:
            new_position = (int64_t)file->inode->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_position < 0) {
        new_position = 0;
    }
    
    if ((uint64_t)new_position > file->inode->size) {
        new_position = file->inode->size;
    }
    
    file->position = (uint32_t)new_position;
    
    return 0;
}

int vfs_register_filesystem(const char* name, const vfs_inode_operations_t* inode_ops,
                           const vfs_superblock_operations_t* sb_ops, uint32_t priority) {
    (void)name;
    (void)inode_ops;
    (void)sb_ops;
    (void)priority;
    return 0;
}

int vfs_unlink(const char* path) {
    (void)path;
    return 0;
}

int vfs_rmdir(const char* path) {
    (void)path;
    return 0;
}

int vfs_stat(const char* path, void* stat_buf) {
    (void)path;
    (void)stat_buf;
    return 0;
}

int vfs_getdents(uint32_t file_id, void* dirent_buffer, size_t size, size_t* bytes_read) {
    (void)file_id;
    (void)dirent_buffer;
    (void)size;
    (void)bytes_read;
    return 0;
}