#include "ext2.h"
#include "logger.h"
#include "string.h"
#include "../gecko/vmm.h"
#include "../gecko/gecko.h"
#include <stdarg.h>

#define EXT2_TAG 0x45585400

static ext2_filesystem_t* mounted_filesystems = NULL;
static int ext2_initialized = 0;

static uint32_t find_free_bit(uint8_t* bitmap, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (bitmap[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(bitmap[i] & (1 << bit))) {
                    return i * 8 + bit;
                }
            }
        }
    }
    return (uint32_t)-1;
}

static uint32_t allocate_block(ext2_filesystem_t* fs) {
    ext2_group_desc_t* group = &fs->group_descs[0];
    uint8_t* block_bitmap = gecko_alloc_kernel_memory(fs->block_size);
    if (!block_bitmap) return (uint32_t)-1;
    
    if (ext2_read_block(fs, group->bg_block_bitmap, block_bitmap) != 0) {
        gecko_free_kernel_memory(block_bitmap);
        return (uint32_t)-1;
    }
    
    uint32_t block_num = find_free_bit(block_bitmap, fs->block_size);
    if (block_num == (uint32_t)-1) {
        gecko_free_kernel_memory(block_bitmap);
        return (uint32_t)-1;
    }
    
    block_bitmap[block_num / 8] |= (1 << (block_num % 8));
    
    if (ext2_write_block(fs, group->bg_block_bitmap, block_bitmap) != 0) {
        gecko_free_kernel_memory(block_bitmap);
        return (uint32_t)-1;
    }
    
    group->bg_free_blocks_count--;
    gecko_free_kernel_memory(block_bitmap);
    
    return group->bg_block_bitmap + 1 + block_num;
}

static uint32_t allocate_inode(ext2_filesystem_t* fs) {
    ext2_group_desc_t* group = &fs->group_descs[0];
    uint8_t* inode_bitmap = gecko_alloc_kernel_memory(fs->block_size);
    if (!inode_bitmap) return (uint32_t)-1;
    
    if (ext2_read_block(fs, group->bg_inode_bitmap, inode_bitmap) != 0) {
        gecko_free_kernel_memory(inode_bitmap);
        return (uint32_t)-1;
    }
    
    uint32_t inode_num = find_free_bit(inode_bitmap, fs->block_size);
    if (inode_num == (uint32_t)-1) {
        gecko_free_kernel_memory(inode_bitmap);
        return (uint32_t)-1;
    }
    
    inode_bitmap[inode_num / 8] |= (1 << (inode_num % 8));
    
    if (ext2_write_block(fs, group->bg_inode_bitmap, inode_bitmap) != 0) {
        gecko_free_kernel_memory(inode_bitmap);
        return (uint32_t)-1;
    }
    
    group->bg_free_inodes_count--;
    gecko_free_kernel_memory(inode_bitmap);
    
    return group->bg_inode_table + inode_num;
}

int ext2_init(void) {
    if (ext2_initialized) {
        return 0;
    }
    
    LOG_INFO("ext2", "initializing ext2 filesystem driver");
    
    mounted_filesystems = NULL;
    ext2_initialized = 1;
    
    LOG_INFO("ext2", "ext2 filesystem driver initialized successfully");
    return 0;
}

int ext2_mount(const char* device) {
    ext2_filesystem_t* fs = gecko_alloc_kernel_memory(sizeof(ext2_filesystem_t));
    if (!fs) {
        LOG_ERROR("ext2", "failed to allocate filesystem structure");
        return -1;
    }
    
    memset(fs, 0, sizeof(ext2_filesystem_t));
    
    fs->device = (void*)device;
    fs->device_size = 1024 * 1024;
    fs->block_size = 1024;
    fs->blocks_per_group = 8192;
    fs->group_count = 1;
    
    fs->superblock = gecko_alloc_kernel_memory(sizeof(ext2_superblock_t));
    if (!fs->superblock) {
        gecko_free_kernel_memory(fs);
        return -1;
    }
    
    memset(fs->superblock, 0, sizeof(ext2_superblock_t));
    fs->superblock->s_magic = EXT2_MAGIC;
    fs->superblock->s_log_block_size = 0;
    fs->superblock->s_inodes_count = 1000;
    fs->superblock->s_blocks_count = 8192;
    fs->superblock->s_free_blocks_count = 7000;
    fs->superblock->s_free_inodes_count = 900;
    fs->superblock->s_blocks_per_group = 8192;
    fs->superblock->s_inodes_per_group = 1000;
    fs->superblock->s_first_ino = 11;
    fs->superblock->s_inode_size = 128;
    
    fs->group_descs = gecko_alloc_kernel_memory(sizeof(ext2_group_desc_t));
    if (!fs->group_descs) {
        gecko_free_kernel_memory(fs->superblock);
        gecko_free_kernel_memory(fs);
        return -1;
    }
    
    memset(fs->group_descs, 0, sizeof(ext2_group_desc_t));
    fs->group_descs->bg_block_bitmap = 3;
    fs->group_descs->bg_inode_bitmap = 4;
    fs->group_descs->bg_inode_table = 5;
    fs->group_descs->bg_free_blocks_count = 7000;
    fs->group_descs->bg_free_inodes_count = 900;
    
    fs->inode_table_start = 5;
    fs->data_block_start = fs->inode_table_start + 100;
    
    ext2_inode_t root_inode;
    memset(&root_inode, 0, sizeof(ext2_inode_t));
    root_inode.i_mode = EXT2_S_IFDIR | 0755;
    root_inode.i_size = 1024;
    root_inode.i_links_count = 2;
    root_inode.i_blocks = 2;
    
    if (ext2_write_inode(fs, EXT2_ROOT_INODE, &root_inode) != 0) {
        LOG_ERROR("ext2", "failed to create root inode");
        gecko_free_kernel_memory(fs->group_descs);
        gecko_free_kernel_memory(fs->superblock);
        gecko_free_kernel_memory(fs);
        return -1;
    }
    
    fs->superblock->s_free_inodes_count--;
    fs->group_descs->bg_free_inodes_count--;
    
    fs->superblock->s_state = 1;
    
    ext2_filesystem_t* current = mounted_filesystems;
    while (current && current->next) {
        current = current->next;
    }
    if (current) {
        current->next = fs;
    } else {
        mounted_filesystems = fs;
    }
    
    LOG_INFO("ext2", "mounted ext2 filesystem on %s", device);
    return 0;
}

int ext2_read_inode(ext2_filesystem_t* fs, uint32_t inode_num, ext2_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock->s_inodes_count) {
        return -1;
    }
    
    uint32_t inode_table_block = fs->inode_table_start;
    uint32_t inode_index = inode_num - 1;
    uint32_t inode_block = inode_table_block + (inode_index * sizeof(ext2_inode_t)) / fs->block_size;
    uint32_t inode_offset = (inode_index * sizeof(ext2_inode_t)) % fs->block_size;
    
    void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
    if (!block_buffer) return -1;
    
    if (ext2_read_block(fs, inode_block, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        return -1;
    }
    
    memcpy(inode, (char*)block_buffer + inode_offset, sizeof(ext2_inode_t));
    gecko_free_kernel_memory(block_buffer);
    
    return 0;
}

int ext2_write_inode(ext2_filesystem_t* fs, uint32_t inode_num, ext2_inode_t* inode) {
    if (inode_num == 0 || inode_num > fs->superblock->s_inodes_count) {
        return -1;
    }
    
    uint32_t inode_table_block = fs->inode_table_start;
    uint32_t inode_index = inode_num - 1;
    uint32_t inode_block = inode_table_block + (inode_index * sizeof(ext2_inode_t)) / fs->block_size;
    uint32_t inode_offset = (inode_index * sizeof(ext2_inode_t)) % fs->block_size;
    
    void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
    if (!block_buffer) return -1;
    
    if (ext2_read_block(fs, inode_block, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        return -1;
    }
    
    memcpy((char*)block_buffer + inode_offset, inode, sizeof(ext2_inode_t));
    
    if (ext2_write_block(fs, inode_block, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        return -1;
    }
    
    gecko_free_kernel_memory(block_buffer);
    return 0;
}

int ext2_read_block(ext2_filesystem_t* fs, uint32_t block_num, void* buffer) {
    if (block_num >= fs->superblock->s_blocks_count) {
        return -1;
    }
    
    size_t offset = block_num * fs->block_size;
    if (offset + fs->block_size > fs->device_size) {
        memset(buffer, 0, fs->block_size);
        return 0;
    }
    
    memcpy(buffer, (char*)fs->device + offset, fs->block_size);
    return 0;
}

int ext2_write_block(ext2_filesystem_t* fs, uint32_t block_num, const void* buffer) {
    if (block_num >= fs->superblock->s_blocks_count) {
        return -1;
    }
    
    size_t offset = block_num * fs->block_size;
    if (offset + fs->block_size > fs->device_size) {
        return -1;
    }
    
    memcpy((char*)fs->device + offset, buffer, fs->block_size);
    return 0;
}

int ext2_find_inode(ext2_filesystem_t* fs, const char* path, uint32_t* inode_num) {
    if (strcmp(path, "/") == 0) {
        *inode_num = EXT2_ROOT_INODE;
        return 0;
    }
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, EXT2_ROOT_INODE, &inode) != 0) {
        return -1;
    }
    
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char* token = strtok(path_copy + 1, "/");
    while (token) {
        ext2_dir_entry_t* dir_entry;
        char dir_buffer[1024];
        size_t bytes_read;
        
        if (ext2_read_directory(fs, inode.i_block[0], dir_buffer, sizeof(dir_buffer), &bytes_read) != 0) {
            return -1;
        }
        
        dir_entry = (ext2_dir_entry_t*)dir_buffer;
        uint32_t offset = 0;
        int found = 0;
        
        while (offset < bytes_read) {
            if (strncmp(dir_entry->name, token, dir_entry->name_len) == 0 && 
                dir_entry->name_len == strlen(token)) {
                *inode_num = dir_entry->inode;
                found = 1;
                break;
            }
            
            offset += dir_entry->rec_len;
            dir_entry = (ext2_dir_entry_t*)((char*)dir_entry + dir_entry->rec_len);
        }
        
        if (!found) {
            return -1;
        }
        
        if (ext2_read_inode(fs, *inode_num, &inode) != 0) {
            return -1;
        }
        
        token = strtok(NULL, "/");
    }
    
    return 0;
}

int ext2_read_directory(ext2_filesystem_t* fs, uint32_t inode_num, void* buffer, size_t size, size_t* bytes_read) {
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    if (size > inode.i_size) {
        size = inode.i_size;
    }
    
    *bytes_read = size;
    return ext2_read_data(fs, inode_num, 0, buffer, size, bytes_read);
}

int ext2_create_file(ext2_filesystem_t* fs, uint32_t parent_inode, const char* name, uint32_t permissions) {
    ext2_inode_t parent_inode_data;
    if (ext2_read_inode(fs, parent_inode, &parent_inode_data) != 0) {
        return -1;
    }
    
    uint32_t new_inode_num = allocate_inode(fs);
    if (new_inode_num == (uint32_t)-1) {
        return -1;
    }
    
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.i_mode = EXT2_S_IFREG | permissions;
    new_inode.i_size = 0;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0;
    new_inode.i_atime = new_inode.i_ctime = new_inode.i_mtime = 0;
    
    if (ext2_write_inode(fs, new_inode_num, &new_inode) != 0) {
        return -1;
    }
    
    size_t name_len = strlen(name);
    size_t entry_size = sizeof(ext2_dir_entry_t) + name_len;
    if (entry_size < 8) entry_size = 8;
    
    ext2_dir_entry_t* new_entry = gecko_alloc_kernel_memory(entry_size);
    if (!new_entry) {
        return -1;
    }
    
    new_entry->inode = new_inode_num;
    new_entry->name_len = name_len;
    new_entry->rec_len = entry_size;
    strncpy(new_entry->name, name, name_len);
    
    uint32_t block_num = parent_inode_data.i_block[0];
    if (block_num == 0) {
        block_num = allocate_block(fs);
        if (block_num == (uint32_t)-1) {
            gecko_free_kernel_memory(new_entry);
            return -1;
        }
        parent_inode_data.i_block[0] = block_num;
        parent_inode_data.i_blocks += fs->block_size / 512;
    }
    
    void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
    if (!block_buffer) {
        gecko_free_kernel_memory(new_entry);
        return -1;
    }
    
    if (ext2_read_block(fs, block_num, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        gecko_free_kernel_memory(new_entry);
        return -1;
    }
    
    memcpy(block_buffer, new_entry, entry_size);
    
    if (ext2_write_block(fs, block_num, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        gecko_free_kernel_memory(new_entry);
        return -1;
    }
    
    gecko_free_kernel_memory(block_buffer);
    gecko_free_kernel_memory(new_entry);
    
    parent_inode_data.i_size += entry_size;
    return ext2_write_inode(fs, parent_inode, &parent_inode_data);
}

int ext2_write_data(ext2_filesystem_t* fs, uint32_t inode_num, uint32_t offset, const void* data, size_t size, size_t* bytes_written) {
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block = (offset + size - 1) / fs->block_size;
    
    for (uint32_t block = start_block; block <= end_block; block++) {
        uint32_t physical_block;
        
        if (block < 12) {
            physical_block = inode.i_block[block];
            if (physical_block == 0) {
                physical_block = allocate_block(fs);
                if (physical_block == (uint32_t)-1) {
                    *bytes_written = 0;
                    return -1;
                }
                inode.i_block[block] = physical_block;
                inode.i_blocks += fs->block_size / 512;
            }
        } else {
            physical_block = allocate_block(fs);
            if (physical_block == (uint32_t)-1) {
                *bytes_written = 0;
                return -1;
            }
        }
        
        void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
        if (!block_buffer) {
            *bytes_written = 0;
            return -1;
        }
        
        if (ext2_read_block(fs, physical_block, block_buffer) != 0) {
            gecko_free_kernel_memory(block_buffer);
            *bytes_written = 0;
            return -1;
        }
        
        size_t block_offset = (block == start_block) ? (offset % fs->block_size) : 0;
        size_t block_size = (block == end_block) ? 
            (size - (block_offset % fs->block_size)) : 
            (fs->block_size - block_offset);
        
        if (block_size > fs->block_size - block_offset) {
            block_size = fs->block_size - block_offset;
        }
        
        memcpy((char*)block_buffer + block_offset, 
               (char*)data + ((block - start_block) * fs->block_size), 
               block_size);
        
        if (ext2_write_block(fs, physical_block, block_buffer) != 0) {
            gecko_free_kernel_memory(block_buffer);
            *bytes_written = 0;
            return -1;
        }
        
        gecko_free_kernel_memory(block_buffer);
    }
    
    if (offset + size > inode.i_size) {
        inode.i_size = offset + size;
    }
    
    ext2_write_inode(fs, inode_num, &inode);
    
    *bytes_written = size;
    return 0;
}

int ext2_read_data(ext2_filesystem_t* fs, uint32_t inode_num, uint32_t offset, void* data, size_t size, size_t* bytes_read) {
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    if (offset >= inode.i_size) {
        *bytes_read = 0;
        return 0;
    }
    
    if (offset + size > inode.i_size) {
        size = inode.i_size - offset;
    }
    
    *bytes_read = size;
    
    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block = (offset + size - 1) / fs->block_size;
    
    for (uint32_t block = start_block; block <= end_block; block++) {
        uint32_t physical_block = (block < 12) ? inode.i_block[block] : 0;
        if (physical_block == 0) {
            memset((char*)data + ((block - start_block) * fs->block_size), 0, fs->block_size);
            continue;
        }
        
        void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
        if (!block_buffer) {
            *bytes_read = 0;
            return -1;
        }
        
        if (ext2_read_block(fs, physical_block, block_buffer) != 0) {
            gecko_free_kernel_memory(block_buffer);
            *bytes_read = 0;
            return -1;
        }
        
        size_t block_offset = (block == start_block) ? (offset % fs->block_size) : 0;
        size_t block_size = (block == end_block) ? 
            (size - (block_offset % fs->block_size)) : 
            (fs->block_size - block_offset);
        
        if (block_size > fs->block_size - block_offset) {
            block_size = fs->block_size - block_offset;
        }
        
        memcpy((char*)data + ((block - start_block) * fs->block_size),
               (char*)block_buffer + block_offset,
               block_size);
        
        gecko_free_kernel_memory(block_buffer);
    }
    
    return 0;
}

int ext2_umount(const char* device) {
    ext2_filesystem_t* prev = NULL;
    ext2_filesystem_t* current = mounted_filesystems;
    
    while (current) {
        if (strcmp(current->device, device) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                mounted_filesystems = current->next;
            }
            
            gecko_free_kernel_memory(current->superblock);
            gecko_free_kernel_memory(current->group_descs);
            gecko_free_kernel_memory(current);
            
            LOG_INFO("ext2", "unmounted ext2 filesystem from %s", device);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1;
}

int ext2_delete_file(ext2_filesystem_t* fs, uint32_t parent_inode, const char* name) {
    ext2_inode_t parent_inode_data;
    if (ext2_read_inode(fs, parent_inode, &parent_inode_data) != 0) {
        return -1;
    }
    
    uint32_t block_num = parent_inode_data.i_block[0];
    void* block_buffer = gecko_alloc_kernel_memory(fs->block_size);
    if (!block_buffer) return -1;
    
    if (ext2_read_block(fs, block_num, block_buffer) != 0) {
        gecko_free_kernel_memory(block_buffer);
        return -1;
    }
    
    ext2_dir_entry_t* dir_entry = (ext2_dir_entry_t*)block_buffer;
    uint32_t offset = 0;
    
    while (offset < parent_inode_data.i_size) {
        if (strncmp(dir_entry->name, name, dir_entry->name_len) == 0 &&
            dir_entry->name_len == strlen(name)) {
            
            uint32_t target_inode = dir_entry->inode;
            
            memset(dir_entry, 0, dir_entry->rec_len);
            parent_inode_data.i_size -= dir_entry->rec_len;
            
            if (ext2_write_block(fs, block_num, block_buffer) != 0) {
                gecko_free_kernel_memory(block_buffer);
                return -1;
            }
            
            ext2_inode_t target_inode_data;
            if (ext2_read_inode(fs, target_inode, &target_inode_data) != 0) {
                gecko_free_kernel_memory(block_buffer);
                return -1;
            }
            
            target_inode_data.i_links_count--;
            if (target_inode_data.i_links_count == 0) {
                for (int i = 0; i < 12; i++) {
                    if (target_inode_data.i_block[i] != 0) {
                        ext2_inode_t inode;
                        memset(&inode, 0, sizeof(ext2_inode_t));
                        ext2_write_inode(fs, target_inode, &inode);
                        break;
                    }
                }
            } else {
                ext2_write_inode(fs, target_inode, &target_inode_data);
            }
            
            ext2_write_inode(fs, parent_inode, &parent_inode_data);
            gecko_free_kernel_memory(block_buffer);
            
            return 0;
        }
        
        offset += dir_entry->rec_len;
        dir_entry = (ext2_dir_entry_t*)((char*)dir_entry + dir_entry->rec_len);
    }
    
    gecko_free_kernel_memory(block_buffer);
    return -1;
}
