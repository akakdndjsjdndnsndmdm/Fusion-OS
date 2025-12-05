#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

#define EXT2_MAGIC 0xEF53
#define EXT2_BLOCK_SIZE_1024 1024
#define EXT2_BLOCK_SIZE_2048 2048
#define EXT2_BLOCK_SIZE_4096 4096
#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_ROOT_INODE 2

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char    s_volume_name[16];
    char    s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint32_t s_reserved[204];
} ext2_superblock_t;

typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} ext2_group_desc_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_reserved1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_reserved2[3];
} ext2_inode_t;

typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint16_t name_len;
    char     name[];
} ext2_dir_entry_t;

#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000
#define EXT2_S_ISUID  0x0800
#define EXT2_S_ISGID  0x0400
#define EXT2_S_ISVTX  0x0200
#define EXT2_S_IRUSR  0x0100
#define EXT2_S_IWUSR  0x0080
#define EXT2_S_IXUSR  0x0040
#define EXT2_S_IRGRP  0x0020
#define EXT2_S_IWGRP  0x0010
#define EXT2_S_IXGRP  0x0008
#define EXT2_S_IROTH  0x0004
#define EXT2_S_IWOTH  0x0002
#define EXT2_S_IXOTH  0x0001

typedef struct ext2_filesystem ext2_filesystem_t;

struct ext2_filesystem {
    void* device;
    size_t device_size;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t group_count;
    ext2_superblock_t* superblock;
    ext2_group_desc_t* group_descs;
    uint32_t inode_table_start;
    uint32_t data_block_start;
    ext2_filesystem_t* next;
};

int ext2_init(void);
int ext2_mount(const char* device);
int ext2_umount(const char* device);
int ext2_read_inode(ext2_filesystem_t* fs, uint32_t inode_num, ext2_inode_t* inode);
int ext2_write_inode(ext2_filesystem_t* fs, uint32_t inode_num, ext2_inode_t* inode);
int ext2_read_block(ext2_filesystem_t* fs, uint32_t block_num, void* buffer);
int ext2_write_block(ext2_filesystem_t* fs, uint32_t block_num, const void* buffer);
int ext2_find_inode(ext2_filesystem_t* fs, const char* path, uint32_t* inode_num);
int ext2_read_directory(ext2_filesystem_t* fs, uint32_t inode_num, void* buffer, size_t size, size_t* bytes_read);
int ext2_create_file(ext2_filesystem_t* fs, uint32_t parent_inode, const char* name, uint32_t permissions);
int ext2_delete_file(ext2_filesystem_t* fs, uint32_t parent_inode, const char* name);
int ext2_write_data(ext2_filesystem_t* fs, uint32_t inode_num, uint32_t offset, const void* data, size_t size, size_t* bytes_written);
int ext2_read_data(ext2_filesystem_t* fs, uint32_t inode_num, uint32_t offset, void* data, size_t size, size_t* bytes_read);

#endif