// inode.c
#include <fs/ext2.h>
#include <drivers/ata.h>
#include <memory/heap.h>
#include <libs/mem_utils.h>
#include <stdbool.h>
#include <error.h>

int ext2_read_inode(Ext2_FS *fs, uint32_t inode_num, Ext2_Inode *out_inode){
    if (inode_num == 0 || inode_num > fs->super_block.inodes_count) return -EINVAL;

    uint32_t group_num = (inode_num - 1) / fs->super_block.inodes_per_group;
    Ext2_GroupDesc *group = &fs->BGDT[group_num];
    uint32_t index = (inode_num - 1) % fs->super_block.inodes_per_group;

    uint64_t byte_offset = (uint64_t)group->inode_table * fs->block_size + (uint64_t)index * fs->inode_size;
    return ext2_read_bytes(fs, byte_offset, sizeof(Ext2_Inode), out_inode);
}

int ext2_write_inode(Ext2_FS *fs, uint32_t inode_num, Ext2_Inode *in_inode){
    if (inode_num > fs->super_block.inodes_count) return -EINVAL;
    uint32_t group_num = (inode_num - 1) / fs->super_block.inodes_per_group;
    Ext2_GroupDesc *group = &fs->BGDT[group_num];
    uint32_t index = (inode_num - 1) % fs->super_block.inodes_per_group;

    uint64_t byte_offset = (uint64_t)group->inode_table * fs->block_size + (uint64_t)index * fs->inode_size;
    return ext2_write_bytes(fs, byte_offset, sizeof(Ext2_Inode), in_inode);
}

int ext2_read_block(Ext2_FS *fs, uint32_t block_num, void *buffer){
    if (block_num >= fs->super_block.blocks_count) return -EINVAL;
    return ext2_read_bytes(fs, (uint64_t)block_num * fs->block_size, fs->block_size, buffer);
}

int ext2_write_block(Ext2_FS *fs, uint32_t block_num, void *buffer){
    if (block_num >= fs->super_block.blocks_count) return -EINVAL;
    return ext2_write_bytes(fs, (uint64_t)block_num * fs->block_size, fs->block_size, buffer);
}

int ext2_read_inode_block(Ext2_FS *fs, Ext2_Inode *inode, uint8_t logical_index, void *buffer){
    if (logical_index >= 12) return -EFBIG;
    uint32_t block_num = inode->block[logical_index];
    return ext2_read_block(fs, block_num, buffer);
}

int ext2_write_inode_block(Ext2_FS *fs, Ext2_Inode *inode, uint8_t logical_index, void *buffer){
    if (logical_index >= 12) return -EFBIG;
    uint32_t block_num = inode->block[logical_index];
    return ext2_write_block(fs ,block_num, buffer);
}

int ext2_read_dir(Ext2_FS *fs, Ext2_Inode *dir_inode, uint32_t *inode_offset, void *dir_entry){
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (block_buffer == NULL) return -ENOMEM;

    uint8_t logical_index = *inode_offset / fs->block_size;
    uint32_t offset = *inode_offset % fs->block_size;

    for (; logical_index < 12; ++logical_index){
        if (dir_inode->block[logical_index] == 0){
            offset = 0;
            continue;
        }
        int r = ext2_read_inode_block(fs, dir_inode, logical_index, block_buffer);
        if (r < 0) {kfree(block_buffer); return r;}

        while (offset < fs->block_size){
            if (offset + sizeof(Ext2_DirEntryHeader) > fs->block_size) return -EIO;

            Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)(block_buffer + offset);
            if (entry->rec_len < sizeof(Ext2_DirEntryHeader) || entry->rec_len % 4 != 0 || offset + entry->rec_len > fs->block_size) return -EIO;

            if (entry->inode != 0){
                memcpy(dir_entry, entry, entry->rec_len);
                *inode_offset = logical_index * fs->block_size + offset + entry->rec_len;
                kfree(block_buffer);
                return 1;
            }
            offset += entry->rec_len;
        }
        offset = 0;
    }
    kfree(block_buffer);
    return 0;
}

int32_t ext2_read(Ext2_FS *fs, uint32_t inode_num, uint32_t offset, void *buffer, uint32_t count){
    Ext2_Inode inode;
    int r = ext2_read_inode(fs, inode_num, &inode);
    if (r < 0) return r;
    if (offset >= inode.size || count == 0) return 0;
    if (count + offset > inode.size) count = inode.size - offset;

    uint32_t logical_index = offset / fs->block_size;
    uint32_t start = offset % fs->block_size;
    uint32_t bytes_read = 0;

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (block_buffer == NULL) return -ENOMEM;

    while (bytes_read < count){
        r = ext2_read_inode_block(fs, &inode, logical_index, block_buffer);
        if (r < 0) {kfree(block_buffer); return r;}

        uint32_t to_copy = (fs->block_size - start < count - bytes_read) ? fs->block_size - start : count - bytes_read;
        memcpy((uint8_t *)buffer + bytes_read, block_buffer + start, to_copy);
        bytes_read += to_copy;
        logical_index++; start = 0;
    }
    kfree(block_buffer);
    return (int32_t)bytes_read;
}

int32_t ext2_write(Ext2_FS *fs, uint32_t inode_num, uint32_t offset, const void *buffer, uint32_t size){
    Ext2_Inode inode;
    int r = ext2_read_inode(fs, inode_num, &inode);
    if (r < 0) return r;
    if (size == 0) return 0;

    uint32_t logical_start = offset / fs->block_size;
    uint32_t start_in_first = offset % fs->block_size;
    const uint8_t *src = (const uint8_t *)buffer;
    uint64_t bytes_done = 0;
    uint32_t logical_index = logical_start;

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size);
    if (block_buf == NULL) return -ENOMEM;

    while (bytes_done < size){
        if (logical_index >= 12) return -EFBIG;   // direct-only scope — the real code for exactly this situation

        uint32_t block_num = inode.block[logical_index];
        bool just_allocated = (block_num == 0);
        if (just_allocated){
            r = ext2_allocate_block(fs, &block_num);
            if (r < 0) {kfree(block_buf); return r;}
            inode.block[logical_index] = block_num;
        }

        uint32_t start_in_block = (logical_index == logical_start) ? start_in_first : 0;
        uint32_t to_write = (fs->block_size - start_in_block < size - bytes_done) ? fs->block_size - start_in_block : size - bytes_done;
        
        if (just_allocated) memset(block_buf, 0, fs->block_size);
        else if (start_in_block != 0 || to_write < fs->block_size){
            r = ext2_read_block(fs, block_num, block_buf);
            if (r < 0) {kfree(block_buf); return r;}
        }

        memcpy(block_buf + start_in_block, src + bytes_done, to_write);
        r = ext2_write_block(fs, block_num, block_buf);
        if (r < 0) {kfree(block_buf); return r;}
        bytes_done += to_write; logical_index++;
    }

    if (offset + size > inode.size) inode.size = offset + size;
    r = ext2_write_inode(fs, inode_num, &inode);
    if (r < 0) {kfree(block_buf); return r;}
    kfree(block_buf);
    return (int64_t)bytes_done;
}
