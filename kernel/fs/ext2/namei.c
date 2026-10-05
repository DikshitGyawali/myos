#include <fs/ext2.h>
#include <memory/heap.h>
#include <libs/string.h>
#include <libs/mem_utils.h>
#include <error.h>

uint32_t ext2_dir_lookup(Ext2_FS *fs, Ext2_Inode *dir_inode, const char *name){
    uint32_t offset = 0;
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (block_buffer == NULL) return -ENOMEM;

    while (ext2_read_dir(fs, dir_inode, &offset, block_buffer)) {
        
        Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)((uint8_t *)block_buffer);
        uint32_t n_len = entry->name_len;
        char temp[n_len + 1];
        memcpy(temp, (char *)entry + sizeof(Ext2_DirEntryHeader), n_len);
        temp[n_len] = '\0';

        if (strcmp(temp, name) == 0){ 
            kfree(block_buffer);
            return entry->inode;
        }
    }
    kfree(block_buffer);
    return 0;
}


static inline uint32_t round_up_4(uint32_t n){ return (n + 3) & ~3u; }

int ext2_add_dir_entry(Ext2_FS *fs, uint32_t parent_dir_num, const char *name, uint32_t inode_num, Ext2_DIR_ENTRY_TYPE file_type){
    uint32_t block_size = fs->block_size;

    Ext2_Inode dir_inode;
    int r = ext2_read_inode(fs, parent_dir_num, &dir_inode);
    if(r < 0) return r;

    uint32_t needed = round_up_4(sizeof(Ext2_DirEntryHeader) + strlen(name));
    uint32_t available = 0;

    uint8_t *block_buf = (uint8_t *)kmalloc(block_size);
    if (block_buf == NULL) return -ENOMEM;

    for (uint8_t i = 0; i < 12; ++i){
        if (dir_inode.block[i] == 0) break;

        
        r = ext2_read_inode_block(fs, &dir_inode, i, block_buf);
        if (r < 0) {kfree(block_buf); return r;}
        uint32_t offset = 0;

        while (offset < block_size){
            Ext2_DirEntryHeader *entry;
            entry =(Ext2_DirEntryHeader *)(block_buf + offset);

            if (entry->inode == 0) available = entry->rec_len;
            else available = entry->rec_len - round_up_4(sizeof(Ext2_DirEntryHeader) + entry->name_len);

            if (available < needed){ offset += entry->rec_len; continue; }

            if (entry->inode == 0){
                if (entry->rec_len - needed >= sizeof(Ext2_DirEntryHeader) + 4){
                    ((Ext2_DirEntryHeader *)(block_buf + offset + needed))->inode = 0;
                    ((Ext2_DirEntryHeader *)(block_buf + offset + needed))->rec_len = entry->rec_len - needed;
                    entry->rec_len = needed;
                }
                entry->inode = inode_num;
            }
            else {
                uint32_t real_size = round_up_4(sizeof(Ext2_DirEntryHeader) + entry->name_len);
                uint32_t new_offset = offset + real_size;

                ((Ext2_DirEntryHeader *)(block_buf + new_offset))->rec_len = entry->rec_len - real_size;
                entry->rec_len = real_size;
                ((Ext2_DirEntryHeader *)(block_buf + new_offset))->inode = inode_num;
                entry = (Ext2_DirEntryHeader *)(block_buf + new_offset);
            }
            entry->name_len = strlen(name);
            entry->file_type = file_type;

            memcpy((uint8_t *)entry + sizeof(Ext2_DirEntryHeader), name, strlen(name));
            r = ext2_write_inode_block(fs, &dir_inode, i, block_buf);
            kfree(block_buf);
            return r;
        }
    }
    kfree(block_buf);

    uint32_t new_block;
    r = ext2_allocate_block(fs, &new_block);
    if (r < 0) return r;

    uint8_t *new_block_buf = (uint8_t *)kmalloc(block_size);
    if (new_block_buf == NULL) return -ENOMEM;

    for (uint8_t j = 0; j < 12; ++j){
        if (dir_inode.block[j] != 0) continue;

        dir_inode.block[j] = new_block;
        dir_inode.size += block_size;
        ext2_write_inode(fs, parent_dir_num, &dir_inode);

        
        Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)new_block_buf;
        entry->inode = inode_num;
        entry->rec_len = block_size;
        entry->name_len = strlen(name);
        entry->file_type = file_type;

        memcpy((uint8_t *)entry + sizeof(Ext2_DirEntryHeader), name, strlen(name));
        r = ext2_write_inode_block(fs, &dir_inode, j, new_block_buf);
        kfree(new_block_buf);
        return r;
    }
    kfree(new_block_buf);
    return -1;
}


int ext2_remove_dir_entry(Ext2_FS *fs, uint32_t parent_dir_num, const char *name){
    uint32_t block_size = fs->block_size;
    Ext2_Inode dir_inode;
    int r = ext2_read_inode(fs, parent_dir_num, &dir_inode);
    if(r < 0) return r;

    uint8_t *block_buf = (uint8_t *)kmalloc(block_size);
    if (block_buf == NULL) return -ENOMEM;

    for (uint8_t i = 0; i < 12; ++i){
        if (dir_inode.block[i] == 0) continue;

        r = ext2_read_inode_block(fs, &dir_inode, i, block_buf);
        if(r < 0) {kfree(block_buf); return r;}

        uint32_t prev_offset = UINT32_MAX;
        uint32_t offset = 0;

        while (offset < block_size){
            Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)block_buf + offset;

            char temp[entry->name_len + 1];
            memcpy(temp, (uint8_t *)entry + sizeof(Ext2_DirEntryHeader), entry->name_len);
            temp[entry->name_len] = '\0';

            if (entry->inode == 0 || strcmp(temp, name) != 0){
                prev_offset = offset;
                offset += entry->rec_len;
                continue;
            }

            if (prev_offset == UINT32_MAX) entry->inode = 0;
            else ((Ext2_DirEntryHeader *)(block_buf + prev_offset))->rec_len += entry->rec_len;

            r = ext2_write_inode_block(fs, &dir_inode, i, block_buf);
            kfree(block_buf);
            return r;
        }
    }
    kfree(block_buf);
    return -ENOENT;
}


int ext2_create(Ext2_FS *fs, uint32_t parent_dir_num, const char *name, Ext2_Inode *out_node){
    Ext2_Inode parent_dir;
    int r = ext2_read_inode(fs, parent_dir_num, &parent_dir);
    if (r < 0) return r;
    if ((parent_dir.mode & 0xF000) != EXT2_S_DIR) return -ENOTDIR;   // wasn't checked before

    if (ext2_dir_lookup(fs, &parent_dir, name) != 0) return -EEXIST;

    uint32_t new_inode_num;
    r = ext2_allocate_inode(fs, &new_inode_num);
    if (r < 0) return r;

    Ext2_Inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.mode = EXT2_S_REG | 0x080 | 0x100; // i tried adding premission, dont know if this helps
    new_inode.size = 0;
    new_inode.links_count = 1;
    for(int8_t i = 0; i <15; ++i) new_inode.block[i] = 0;

    r = ext2_write_inode(fs, new_inode_num, &new_inode);
    if (r < 0) return r;

    r = ext2_add_dir_entry(fs, parent_dir_num, name, new_inode_num, 1);
    if (r < 0) return r;

    *out_node = new_inode;
    return new_inode_num;
}


int ext2_mkdir(Ext2_FS *fs, uint32_t parent_dir_num, const char *name){
    uint32_t block_size = fs->block_size;
    Ext2_Inode parent_inode;
    int r = ext2_read_inode(fs, parent_dir_num, &parent_inode);
    if (r < 0) return r;
    if ((parent_inode.mode & 0xF000) != EXT2_S_DIR) return -ENOTDIR;


    if (ext2_dir_lookup(fs, &parent_inode, name) != 0) return -EEXIST;
    uint32_t new_inode_num;
    r = ext2_allocate_inode(fs, &new_inode_num);
    if (r < 0) return r;
    uint32_t new_block_num;
    r = ext2_allocate_block(fs, &new_block_num);
    if (r < 0){ return ext2_free_inode(fs, new_inode_num);}

    Ext2_Inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.mode = EXT2_S_DIR | 0x080| 0x040 | 0x100;
    new_inode.size = block_size;
    new_inode.links_count = 2;
    new_inode.block[0] = new_block_num;
    for(int8_t i = 1; i <15; ++i) new_inode.block[i] = 0;
    r = ext2_write_inode(fs, new_inode_num, &new_inode);
    if(r < 0) return r;
    
    uint8_t *new_block_buf = (uint8_t *)kmalloc(block_size);
    if (new_block_buf == NULL) return -ENOMEM;
    memset(new_block_buf, 0, block_size);

    Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)new_block_buf;
    entry->file_type = DIRECTORY;
    entry->inode = new_inode_num;
    entry->name_len = 1u;
    entry->rec_len = round_up_4(sizeof(Ext2_DirEntryHeader) + 1);
    memset((uint8_t *)entry + sizeof(Ext2_DirEntryHeader), '.', 1);

    uint32_t offset = entry->rec_len;
    entry = (Ext2_DirEntryHeader *)(new_block_buf + offset);
    entry->file_type = DIRECTORY;
    entry->inode = parent_dir_num;
    entry->name_len = 2u;
    entry->rec_len = block_size - offset;
    memset((uint8_t *)entry + sizeof(Ext2_DirEntryHeader), '.', 2);
    r = ext2_write_block(fs, new_block_num, new_block_buf);
    if(r < 0) {kfree(new_block_buf); return r;}

    r = ext2_add_dir_entry(fs, parent_dir_num, name, new_inode_num, DIRECTORY);
    if(r < 0) {kfree(new_block_buf); return r;}

    parent_inode.links_count += 1;
    kfree(new_block_buf);
    return ext2_write_inode(fs, parent_dir_num, &parent_inode);
}


int ext2_unlink(Ext2_FS *fs, uint32_t parent_dir_num, const char *name){
    Ext2_Inode dir_inode;
    int r = ext2_read_inode(fs, parent_dir_num, &dir_inode);
    if(r < 0) return r;

    if ((dir_inode.mode & 0xF000) == EXT2_S_DIR) return -EISDIR;

    uint32_t inode_num = ext2_dir_lookup(fs, &dir_inode, name);
    if (inode_num == 0) return -ENOENT;

    r = ext2_remove_dir_entry(fs, parent_dir_num, name);
    if (r < 0) return r;

    Ext2_Inode inode;
    r = ext2_read_inode(fs, inode_num, &inode);
    if (r < 0) return r;
    inode.links_count--;

    if (inode.links_count != 0) return ext2_write_inode(fs, inode_num, &inode);

    for (uint8_t i = 0; i < 12; ++i){
        if (inode.block[i] == 0) continue;
        r = ext2_free_block(fs, inode.block[i]);
        if (r < 0) return r;
    }
    r = ext2_free_inode(fs, inode_num);
    if (r < 0) return r;

    return 0;
}

static bool ext2_dir_is_empty(Ext2_FS *fs, Ext2_Inode *dir_inode){
    uint32_t offset = 0;
    uint8_t *buffer = (uint8_t *)kmalloc(sizeof(fs->block_size));
    if (buffer == NULL) return -ENOMEM;
    uint32_t count = 0;
    while (ext2_read_dir(fs, dir_inode, &offset, buffer)) count++;
    kfree(buffer);
    return count <= 2;
}

int ext2_rmdir(Ext2_FS *fs, uint32_t parent_dir_num, const char *name){
    Ext2_Inode parent_dir;
    int r = ext2_read_inode(fs, parent_dir_num, &parent_dir);
    if(r < 0) return r;

    uint32_t inode_num = ext2_dir_lookup(fs, &parent_dir, name);
    if (inode_num == 0) return -ENOENT;

    Ext2_Inode inode;
    r = ext2_read_inode(fs, inode_num, &inode);
    if(r < 0) return r;
    if ((inode.mode & 0xF000) != EXT2_S_DIR) return -ENOTDIR;
    if(!ext2_dir_is_empty(fs, &inode)) return -ENOTEMPTY;

    r = ext2_remove_dir_entry(fs, parent_dir_num, name);
    if (r < 0) return r;

    parent_dir.links_count--;

    r = ext2_write_inode(fs, parent_dir_num, &parent_dir);
    if(r < 0) return r;

    for (uint8_t i = 0; i < 12; ++i){
        if (inode.block[i] == 0) continue;
        int r = ext2_free_block(fs, inode.block[i]);
        if (r < 0) return r;
    }
    return ext2_free_inode(fs, inode_num);
}


static int ext2_update_dotdot(Ext2_FS *fs, uint32_t dir_inode_num, uint32_t new_parent_inode_num){
    Ext2_Inode dir_inode;
    int r = ext2_read_inode(fs, dir_inode_num, &dir_inode);
    if (r < 0) return r;

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (block_buffer == NULL) return -ENOMEM;

    r = ext2_read_inode_block(fs, &dir_inode,0,block_buffer);
    if (r < 0) {kfree(block_buffer); return r;}

    uint32_t offset = 0;
    while (ext2_read_dir(fs, &dir_inode, &offset, block_buffer)){
        Ext2_DirEntryHeader *entry = (Ext2_DirEntryHeader *)block_buffer;

        if (entry->name_len == 2 && memcmp((uint8_t *)entry + sizeof(Ext2_DirEntryHeader), "..", 2) == 0){
            entry->inode = new_parent_inode_num;
            r = ext2_write_block(fs, dir_inode.block[0], block_buffer);
            kfree(block_buffer);
            return r;
        }
    }
    kfree(block_buffer);
    return -ENOENT;
}

int ext2_rename(Ext2_FS *fs, uint32_t old_parent_inode, const char *old_name, uint32_t new_parent_inode, const char *new_name){
    if (old_name == NULL || new_name == NULL) return -ENOENT;
    if (old_name[0] == '\0' || new_name[0] == '\0') return -ENOENT;
    if (strcmp(old_name, ".") == 0 || strcmp(old_name, "..") == 0) return -ENOENT;
    if (strcmp(new_name, ".") == 0 || strcmp(new_name, "..") == 0) return -ENOENT;

    Ext2_Inode old_parent;
    int r = ext2_read_inode(fs, old_parent_inode, &old_parent);
    if (r < 0) return r;

    uint32_t target_inode_num = ext2_dir_lookup(fs, &old_parent, old_name);
    if (target_inode_num == 0) return -ENOENT;

    Ext2_Inode new_parent;
    r = ext2_read_inode(fs, new_parent_inode, &new_parent);
    if (r < 0) return r;
    uint32_t existing_inode_num = ext2_dir_lookup(fs, &new_parent, new_name);
    if (existing_inode_num != 0) return -ENOENT;

    Ext2_Inode target_inode;
    r = ext2_read_inode(fs, target_inode_num, &target_inode);
    if (r < 0) return r;

    uint8_t file_type;
    if ((target_inode.mode & 0xF000) == EXT2_S_DIR) file_type = DIRECTORY;
    else file_type = FILE;

    r = ext2_add_dir_entry(fs, new_parent_inode, new_name, target_inode_num, file_type);
    if (r < 0) return r;

    if (file_type == DIRECTORY && old_parent_inode != new_parent_inode){
        r = ext2_update_dotdot(fs, target_inode_num, new_parent_inode);
        if (r < 0) return r;
    }

    r = ext2_remove_dir_entry(fs, old_parent_inode, old_name);
    if (r < 0) return r;

    if (file_type == DIRECTORY && old_parent_inode != new_parent_inode){
        Ext2_Inode old_parent;
        Ext2_Inode new_parent;
        r = ext2_read_inode(fs, old_parent_inode, &old_parent);
        if (r < 0) return r;
        r = ext2_read_inode(fs, new_parent_inode, &new_parent);
        if (r < 0) return r;

        old_parent.links_count--;
        new_parent.links_count++;

        r = ext2_write_inode(fs, old_parent_inode, &old_parent);
        if (r < 0) return r;
        r = ext2_write_inode(fs, new_parent_inode, &new_parent);
        if (r < 0) return r;
    }
    return 0;
}
