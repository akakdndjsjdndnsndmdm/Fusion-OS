#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_PATH_LENGTH 256
#define VFS_MAX_FILENAME_LENGTH 64
#define VFS_MAX_FILE_DESCRIPTORS 64
#define VFS_MAX_MOUNT_POINTS 32

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef enum {
    VFS_TYPE_FILE,
    VFS_TYPE_DIRECTORY,
    VFS_TYPE_DEVICE,
    VFS_TYPE_PIPE,
    VFS_TYPE_SOCKET
} vfs_type_t;

typedef enum {
    VFS_PERM_READ = 1,
    VFS_PERM_WRITE = 2,
    VFS_PERM_EXECUTE = 4
} vfs_permissions_t;

typedef enum {
    VFS_O_RDONLY = 0x00000001,
    VFS_O_WRONLY = 0x00000002,
    VFS_O_RDWR = 0x00000003,
    VFS_O_CREAT = 0x00000010,
    VFS_O_EXCL = 0x00000020,
    VFS_O_TRUNC = 0x00000040,
    VFS_O_APPEND = 0x00000080
} vfs_open_flags_t;

typedef struct vfs_file vfs_file_t;
typedef struct vfs_inode vfs_inode_t;
typedef struct vfs_superblock vfs_superblock_t;
typedef struct vfs_mount_point vfs_mount_point_t;

typedef struct vfs_file_operations {
    int (*open)(vfs_file_t* file, const char* path, uint32_t flags);
    int (*read)(vfs_file_t* file, void* buffer, size_t size, size_t* bytes_read);
    int (*write)(vfs_file_t* file, const void* buffer, size_t size, size_t* bytes_written);
    int (*close)(vfs_file_t* file);
    int (*seek)(vfs_file_t* file, int64_t offset, int whence);
    int (*stat)(vfs_file_t* file, void* stat_buf);
    int (*unlink)(vfs_file_t* file, const char* path);
} vfs_file_operations_t;

typedef struct vfs_inode_operations {
    int (*mkdir)(vfs_inode_t* parent, const char* name, uint32_t permissions);
    int (*rmdir)(vfs_inode_t* parent, const char* name);
    int (*link)(vfs_inode_t* target, const char* link_name);
    int (*unlink)(vfs_inode_t* parent, const char* name);
    int (*create_file)(vfs_inode_t* parent, const char* name, uint32_t permissions);
} vfs_inode_operations_t;

typedef struct vfs_superblock_operations {
    int (*mount)(vfs_superblock_t* sb, const char* device, const char* mount_point);
    int (*umount)(vfs_superblock_t* sb);
    int (*sync)(vfs_superblock_t* sb);
} vfs_superblock_operations_t;

struct vfs_inode {
    uint32_t inode_id;
    vfs_type_t type;
    uint32_t permissions;
    uint32_t size;
    uint32_t link_count;
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t access_time;
    void* data;
    const vfs_inode_operations_t* ops;
    vfs_superblock_t* sb;
    int reference_count;
};

struct vfs_superblock {
    const char* device_name;
    const char* mount_point;
    vfs_inode_t* root_inode;
    const vfs_superblock_operations_t* ops;
    void* data;
    int reference_count;
};

struct vfs_file {
    uint32_t file_id;
    vfs_inode_t* inode;
    uint32_t position;
    uint32_t flags;
    const vfs_file_operations_t* ops;
    void* private_data;
    int reference_count;
};

struct vfs_mount_point {
    const char* path;
    const char* mount_point;
    const char* device_name;
    vfs_superblock_t* superblock;
    vfs_inode_t* mount_inode;
    int active;
};

typedef struct {
    vfs_file_t* files;
    uint32_t used_count;
} vfs_file_table_t;

int vfs_init(void);
int vfs_mount(const char* device, const char* mount_point, const char* fs_type);
int vfs_umount(const char* mount_point);
vfs_inode_t* vfs_lookup(const char* path);
int vfs_open(const char* path, uint32_t flags, uint32_t file_id);
int vfs_close(uint32_t file_id);
int vfs_read(uint32_t file_id, void* buffer, size_t size, size_t* bytes_read);
int vfs_write(uint32_t file_id, const void* buffer, size_t size, size_t* bytes_written);
int vfs_mkdir(const char* path, uint32_t permissions);
int vfs_rmdir(const char* path);
int vfs_unlink(const char* path);
int vfs_stat(const char* path, void* stat_buf);
int vfs_getdents(uint32_t file_id, void* dirent_buffer, size_t size, size_t* bytes_read);
int vfs_seek(uint32_t file_id, int64_t offset, int whence);
int vfs_register_filesystem(const char* name, const vfs_inode_operations_t* inode_ops,
                           const vfs_superblock_operations_t* sb_ops, uint32_t priority);
vfs_superblock_t* vfs_get_superblock(const char* path);

typedef struct {
    char name[VFS_MAX_FILENAME_LENGTH];
    vfs_type_t type;
    uint32_t size;
    uint32_t inode_id;
} vfs_dirent_t;

#endif