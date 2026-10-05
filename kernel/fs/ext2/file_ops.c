#include <fs/ext2.h>
#include <fs/vfs.h>
#include <libs/mem_utils.h>
#include <error.h>
#include <stdbool.h>
#include <stdint.h>


int64_t vfs_ext2_read(VFS_Node *node, uint32_t offset, void *buffer, uint32_t count){
    if (node->is_directory) return -EISDIR;
    return ext2_read(node->mount->fs_data,(uint32_t)(node->fs_data), offset, buffer, count); 
}

int64_t vfs_ext2_write(VFS_Node *node, uint32_t offset, const void *buffer, uint32_t count){
    if (node->is_directory) return -EISDIR;
    return ext2_write(node->mount->fs_data, (uint32_t)(node->fs_data), offset, buffer, count); 
}
