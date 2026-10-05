#include <fs/ext2.h>
#include <fs/vfs.h>
#include <drivers/ata.h>
#include <IO/screen.h>
#include <memory/heap.h>
#include <libs/mem_utils.h>
#include <panic.h>
#include <error.h>
#include <stdbool.h>


FilesystemDriver ext2_driver = {
    .init = vfs_ext2_init, 
    .namespace_ops = {
        .lookup_one     = vfs_ext2_lookup_one,
        .dir_list       = vfs_ext2_list_dir,
        .mkdir          = vfs_ext2_mkdir,
        .create         = vfs_ext2_create,
        .unlink         = vfs_ext2_unlink,
        .rmdir          = vfs_ext2_rmdir, 
        .rename         = vfs_ext2_rename,
    },
    .file_ops = {
        .read = vfs_ext2_read,
        .write = vfs_ext2_write,
        .truncate = NULL,
    }
};


static int ext2_init(VFS_Mount *mount, Ext2_Inode *out_inode){
    Ext2_FS *fs = (Ext2_FS *)kmalloc(sizeof(Ext2_FS));
    if (fs == NULL) return -ENOMEM;
    fs->device = mount->device;
    uint8_t *temp = (uint8_t *)kmalloc(SECTOR_SIZE);
    if (temp == NULL){kfree(fs); return -ENOMEM;}
    fs->device->read_sector(fs->device, 2, temp);
    memcpy(&(fs->super_block), temp, sizeof(Ext2_Superblock));
    kfree(temp);
    if ((fs->super_block).magic != 0xEF53){
        kprintf("Validation failed for magic number of Ext2\n");
        kfree(fs);
        return -EINVAL;
    }
    if (fs->super_block.rev_level >= 1) fs->inode_size = fs->super_block.inode_size;
    else fs->inode_size = 128u;

    fs->block_size = 1024 << fs->super_block.log_block_size; // 1024
    fs->group_count = (fs->super_block.blocks_count + fs->super_block.blocks_per_group - 1) / fs->super_block.blocks_per_group;

    uint32_t BGDT_size = fs->group_count * sizeof(Ext2_GroupDesc);
    uint32_t BGDT_start_block = (fs->block_size == 1024)? 2 : 1;
    fs->BGDT_start = BGDT_start_block * fs->block_size;
    
    fs->BGDT = kmalloc(BGDT_size);
    if (fs->BGDT == NULL){ kprintf("Failed to allocate memory for BGDT\n"); kfree(fs); return -ENOMEM;}

    int r = ext2_read_bytes(fs, fs->BGDT_start, BGDT_size, fs->BGDT);
    if(r < 0){ kfree(fs->BGDT); kfree(fs); return r; }

    r = ext2_read_inode(fs, 2, out_inode);
    if(r < 0) { kfree(fs->BGDT); kfree(fs); return r; }

    mount->fs_data = fs;
    return true;
}

int vfs_ext2_init(VFS_Mount *mount, VFS_Node *out_root){

    Ext2_Inode inode;
    int r = ext2_init(mount, &inode);
    if (r < 0) return r;
    mount->driver = &ext2_driver;
    mount->root = out_root;
    out_root->fs_data = (void*)(uintptr_t)2;
    out_root->is_directory = true;
    out_root->size = inode.size;
    out_root->mount = mount;
    return 0;
}

int ext2_read_bytes(Ext2_FS *fs, uint64_t byte_offset, uint64_t length, void *buffer){
    uint8_t *temp = (uint8_t *)kmalloc(SECTOR_SIZE);
    if (temp == NULL) return -ENOMEM;

    uint64_t sector_start = byte_offset / SECTOR_SIZE;
    uint32_t sector_offset = byte_offset % SECTOR_SIZE;

    uint8_t *dest = (uint8_t *)buffer;

    while (length > 0) {

        if (!fs->device->read_sector(fs->device, sector_start, (uint16_t *)temp)) {kfree(temp); return -EIO;}

        uint32_t available = SECTOR_SIZE - sector_offset;
        uint32_t copy_size = length < available ? length : available;

        memcpy(dest, temp + sector_offset, copy_size);

        dest += copy_size;
        length -= copy_size;

        sector_start++;
        sector_offset = 0;
    }
    kfree(temp);
    return 0;
}

int ext2_write_bytes(Ext2_FS *fs, uint64_t byte_offset, uint64_t length, void *buffer){
    uint8_t *temp = (uint8_t *)kmalloc(SECTOR_SIZE);
    if (temp == NULL) return -ENOMEM;

    uint64_t sector_start = byte_offset / SECTOR_SIZE;
    uint32_t sector_offset = byte_offset % SECTOR_SIZE;
    uint8_t *src = (uint8_t *)buffer;

    uint64_t sector_count = 0;

    while (length > 0){
        uint32_t available = SECTOR_SIZE - sector_offset;
        uint32_t copy_size = (length < available) ? length : available;

        if (copy_size < SECTOR_SIZE)
            if (!fs->device->read_sector(fs->device, sector_start + sector_count, (uint16_t *)temp)) {kfree(temp); return -EIO;}

        memcpy(temp + sector_offset, src, copy_size);
        if (!fs->device->write_sector(fs->device, sector_start + sector_count, temp)) {kfree(temp); return -EIO;}

        src += copy_size;
        length -= copy_size;
        sector_count++;
        sector_offset = 0;
    }
    kfree(temp);
    return 0;
}
