#include <fs/ext2.h>
#include <fs/vfs.h>
#include <memory/heap.h>
#include <libs/mem_utils.h>
#include <error.h>


bool vfs_ext2_lookup_one(VFS_Node *dir, const char *name, VFS_Node *out_node){
    if (!dir->is_directory) return false;
    Ext2_FS *fs = (Ext2_FS *)dir->mount->fs_data;

    Ext2_Inode dir_inode;
    if (ext2_read_inode(fs, (uint32_t)(uintptr_t)dir->fs_data, &dir_inode) < 0) return false;

    uint32_t result_num = ext2_dir_lookup(fs, &dir_inode, name);
    if (result_num == 0) return false;

    Ext2_Inode inode;
    if (ext2_read_inode(fs, result_num, &inode) < 0) return false;

    out_node->fs_data = (void*)(uintptr_t)result_num;
    out_node->size = inode.size;
    out_node->is_directory = (inode.mode & 0xF000) == EXT2_S_DIR;
    out_node->mount = dir->mount;
    return true;
}

int vfs_ext2_list_dir(VFS_Node *dir, Callback callback){
    if (!dir->is_directory) return -ENOTDIR;
    Ext2_FS *fs = (Ext2_FS *)dir->mount->fs_data;
    uint32_t offset = 0;
    uint32_t inode_num = (uint32_t)(uintptr_t)dir->fs_data;
    Ext2_Inode inode;
    uint8_t *block_buffer = (uint8_t *)kmalloc(sizeof(fs->block_size));
    if (block_buffer == NULL) return -ENOMEM;

    int r = ext2_read_inode(fs, inode_num, &inode);
    if(r < 0) {kfree(block_buffer); return r;}

    while (ext2_read_dir(fs, &inode, &offset, block_buffer)) {
        
        Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)((uint8_t *)block_buffer);
        uint32_t n_len = entry->name_len;
        char temp[n_len + 1];
        memcpy(temp, (char *)entry + sizeof(Ext2_DirEntryHeader), n_len);
        temp[n_len] = '\0';

        callback(temp, (entry->file_type == 2));
    }
    kfree(block_buffer);
    return 0;
}

int vfs_ext2_create(VFS_Node *parent, const char *name, VFS_Node *out_node){
    if (!parent->is_directory) return -ENOTDIR;

    Ext2_Inode out_inode;
    int out_inode_num = ext2_create(parent->mount->fs_data, (uint32_t)(parent->fs_data), name, &out_inode);
    if (out_inode_num < 0) return out_inode_num;

    out_node->fs_data = (void*)(uintptr_t)out_inode_num;
    out_node->is_directory = (out_inode.mode & 0xF000) == EXT2_S_DIR;
    out_node->size = out_inode.size;
    out_node->mount = parent->mount;
    return 0;
}

int vfs_ext2_mkdir(VFS_Node *parent, const char *name){
    if (!parent->is_directory) return -ENOTDIR;
    return ext2_mkdir(parent->mount->fs_data, (uint32_t)(parent->fs_data), name);
}

int vfs_ext2_unlink(VFS_Node *parent, const char *name){
    if(!parent->is_directory) return -ENOTDIR;
    return ext2_unlink(parent->mount->fs_data, (uint32_t)(parent->fs_data), name);
}

int vfs_ext2_rmdir(VFS_Node *parent, const char *name){
    if(!parent->is_directory) return -ENOTDIR;
    return ext2_rmdir(parent->mount->fs_data, (uint32_t)(parent->fs_data), name);
}

int vfs_ext2_rename(VFS_Node *old_parent, const char *old_name, VFS_Node *new_parent, const char *new_name){
    if(!old_parent->is_directory) return -ENOTDIR;
    if(!new_parent->is_directory) return -ENOTDIR;

    if(old_parent->mount->fs_data != new_parent->mount->fs_data) return -EXDEV;

    return ext2_rename(old_parent->mount->fs_data, (uint32_t)(old_parent->fs_data), old_name, (uint32_t)(new_parent->fs_data), new_name);
}
