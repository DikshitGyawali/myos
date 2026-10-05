#include <drivers/ata.h>
#include <memory/heap.h>
#include <fs/ext2.h>
#include <error.h>

static inline void SetBit(uint8_t *bitmap, uint32_t i){ bitmap[i/8] |= (1 << (i%8)); }
static inline void ClearBit(uint8_t *bitmap, uint32_t i){ bitmap[i/8] &= ~(1 << (i%8)); }
static inline bool TestBit(uint8_t *bitmap, uint32_t i){ return (bitmap[i/8] >> (i%8)) & 1; }


int ext2_allocate_block(Ext2_FS *fs, uint32_t *out_block_num){
    uint8_t *bitmap_buf = (uint8_t *)kmalloc(fs->block_size);
    if (bitmap_buf == NULL) return -ENOMEM;

    for (uint32_t group_num = 0; group_num < fs->group_count; ++group_num){
        if (fs->BGDT[group_num].free_blocks_count == 0) continue;

        uint32_t blocks_in_this_group = (fs->super_block.blocks_per_group < fs->super_block.blocks_count - group_num * fs->super_block.blocks_per_group)?
                fs->super_block.blocks_per_group : 
                fs->super_block.blocks_count - group_num * fs->super_block.blocks_per_group;

        int r = ext2_read_block(fs, fs->BGDT[group_num].block_bitmap, bitmap_buf);
        if(r < 0){kfree(bitmap_buf); return r;}

        for (uint32_t i = 0; i < blocks_in_this_group; ++i){
            if (TestBit(bitmap_buf, i)) continue;

            SetBit(bitmap_buf, i);
            r = ext2_write_block(fs, fs->BGDT[group_num].block_bitmap, bitmap_buf);
            if (r < 0) {kfree(bitmap_buf); return r;}

            fs->BGDT[group_num].free_blocks_count -= 1;
            r = ext2_write_bytes(fs, 1024, fs->group_count * sizeof(Ext2_GroupDesc), fs->BGDT);
            if (r < 0) {kfree(bitmap_buf); return r;}

            fs->super_block.free_blocks_count -= 1;
            r = ext2_write_bytes(fs, 1024, sizeof(Ext2_Superblock), &fs->super_block);
            if (r < 0) {kfree(bitmap_buf); return r;}

            *out_block_num = group_num * fs->super_block.blocks_per_group + i + fs->super_block.first_data_block;
            kfree(bitmap_buf);
            return 0;
        }  
    }
    kfree(bitmap_buf);
    return -ENOSPC;
}


int ext2_free_block(Ext2_FS *fs, uint32_t block_num){
    if (block_num < fs->super_block.first_data_block || block_num >= fs->super_block.blocks_count)
        return -EINVAL;

    uint32_t adjusted_block_num = block_num - fs->super_block.first_data_block;
    uint32_t group_num = adjusted_block_num / fs->super_block.blocks_per_group;
    uint32_t i = adjusted_block_num % fs->super_block.blocks_per_group;

    uint8_t *bitmap_buf = (uint8_t *)kmalloc(fs->block_size);
    if (bitmap_buf == NULL) return -ENOMEM;
    
    int r = ext2_read_block(fs, fs->BGDT[group_num].block_bitmap, bitmap_buf);
    if(r < 0) {kfree(bitmap_buf); return r;}

    ClearBit(bitmap_buf, i);
    r = ext2_write_block(fs, fs->BGDT[group_num].block_bitmap, bitmap_buf);
    if(r < 0) {kfree(bitmap_buf); return r;}

    fs->BGDT[group_num].free_blocks_count += 1;
    r = ext2_write_bytes(fs, fs->BGDT_start, fs->group_count * sizeof(Ext2_GroupDesc), fs->BGDT);
    if(r < 0) {kfree(bitmap_buf); return r;}

    fs->super_block.free_blocks_count += 1;

    kfree(bitmap_buf);
    return ext2_write_bytes(fs, 1024, sizeof(Ext2_Superblock), &fs->super_block);
}


int ext2_allocate_inode(Ext2_FS *fs, uint32_t *out_inode_num){

    uint8_t *bitmap_buf = (uint8_t *)kmalloc(fs->block_size);
    if (bitmap_buf == NULL) return -ENOMEM;

    for (uint32_t group_num = 0; group_num < fs->group_count; ++group_num){
        if (fs->BGDT[group_num].free_inodes_count == 0) continue;

        int r = ext2_read_block(fs, fs->BGDT[group_num].inode_bitmap, bitmap_buf);
        if (r < 0) {kfree(bitmap_buf); return r;}

        for (uint32_t i = 0, n= fs->super_block.inodes_per_group; i < n; ++i){
            if (TestBit(bitmap_buf, i)) continue;

            SetBit(bitmap_buf, i);
            r = ext2_write_block(fs, fs->BGDT[group_num].inode_bitmap, bitmap_buf);
            if (r < 0) {kfree(bitmap_buf); return r;}

            fs->BGDT[group_num].free_inodes_count -= 1;
            r = ext2_write_bytes(fs, fs->BGDT_start, fs->group_count * sizeof(Ext2_GroupDesc), fs->BGDT);
            if (r < 0) {kfree(bitmap_buf); return r;}

            fs->super_block.free_inodes_count -= 1;
            r = ext2_write_bytes(fs, 1024, sizeof(Ext2_Superblock), &fs->super_block);
            if (r < 0) {kfree(bitmap_buf); return r;}

            *out_inode_num = group_num * fs->super_block.inodes_per_group + i + 1;
            kfree(bitmap_buf);
            return 0;
        }
    }
    kfree(bitmap_buf);
    return -ENOSPC;
}

int ext2_free_inode(Ext2_FS *fs, uint32_t inode_num){
    if (inode_num < 2 || inode_num >= fs->super_block.inodes_count) return -EINVAL;

    uint32_t adjusted_inode_num = inode_num - 1;
    uint32_t group_num = adjusted_inode_num / fs->super_block.inodes_per_group;
    uint32_t i = adjusted_inode_num % fs->super_block.inodes_per_group;

    uint8_t *bitmap_buf = (uint8_t *)kmalloc(fs->block_size);
    if (bitmap_buf == NULL) return -ENOMEM;

    int r = ext2_read_block(fs, fs->BGDT[group_num].inode_bitmap, bitmap_buf);
    if(r < 0) {kfree(bitmap_buf); return r;}

    ClearBit(bitmap_buf, i);
    r = ext2_write_block(fs, fs->BGDT[group_num].inode_bitmap, bitmap_buf);
    if(r < 0) {kfree(bitmap_buf); return r;}

    fs->BGDT[group_num].free_inodes_count += 1;
    r = ext2_write_bytes(fs, fs->BGDT_start, fs->group_count * sizeof(Ext2_GroupDesc), fs->BGDT);
    if(r < 0) {kfree(bitmap_buf); return r;}

    fs->super_block.free_inodes_count += 1;
    kfree(bitmap_buf);
    return ext2_write_bytes(fs, 1024, sizeof(Ext2_Superblock), &fs->super_block);;
}
