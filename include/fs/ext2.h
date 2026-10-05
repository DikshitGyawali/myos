#ifndef EXT2_H
#define EXT2_H

#include <fs/vfs.h>
#include <block/block.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{
    // ---- Base fields (bytes 0-83), always present ----
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;       // blocks reserved for superuser
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;     // block holding the superblock itself
    uint32_t log_block_size;       // real block size = 1024 << this
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;                // must equal 0xEF53
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;            // 0 or 1 — gates everything below
    uint16_t def_resuid;
    uint16_t def_resgid;

    // ---- Extended fields (bytes 84-235), only meaningful if rev_level >= 1 ----
    uint32_t first_ino;
    uint16_t inode_size;           // real inode size when rev_level >= 1
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;     // bit 0x0002 = dirents carry a type byte
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    uint8_t  volume_name[16];      // C string
    uint8_t  last_mounted[64];     // C string
    uint32_t algo_bitmap;
    uint8_t  prealloc_blocks;
    uint8_t  prealloc_dir_blocks;
    uint16_t _unused0;
    uint8_t  journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    // bytes 236-1023, unused
}__attribute__((packed)) Ext2_Superblock;

typedef struct{
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint8_t  reserved[14];
}__attribute__((packed)) Ext2_GroupDesc;                  // 32 bytes total

typedef struct {
    uint16_t mode;          // top nibble = type, bottom 12 bits = permissions
    uint16_t uid;
    uint32_t size;          // lower 32 bits
    uint32_t atime;
    uint32_t ctime;         // creation time, i think
    uint32_t mtime;
    uint32_t dtime;         // 0 unless deleted
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;  
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];     // 12 direct, then single/double/triple indirect
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;       // or size_high for large files
    uint32_t faddr;
    uint8_t  osd2[12];
} __attribute__((packed)) Ext2_Inode;                // 128 bytes total

// mode's top nibble — check with (mode & 0xF000)
#define EXT2_S_DIR  0x4000
#define EXT2_S_REG  0x8000
#define EXT2_S_LNK  0xA000

typedef struct {
    uint32_t inode;      // 0 means "empty, skip this entry"
    uint16_t rec_len;    // total size of this entry — use to reach the next one
    uint8_t  name_len;
    uint8_t  file_type;  // only valid if feature_incompat & 0x0002
    // name here, name_len bytes, not null-terminated
} __attribute__((packed)) Ext2_DirEntryHeader;

typedef enum{
    UNKNOWN = 0,
    FILE = 1,
    DIRECTORY = 2,
    CHARACTER_DEVICE = 3, 
    BLOCK_DEVICE = 4,
    FIFO = 5,
    SOCKET = 6,
    SYMBOLIC_LINK = 7,
} Ext2_DIR_ENTRY_TYPE;


#define SECTOR_SIZE 512


typedef struct {
    Ext2_Superblock super_block;
    Ext2_GroupDesc *BGDT;
    uint32_t BGDT_start;
    uint16_t inode_size;   // right now 256
    uint32_t block_size;
    uint32_t group_count;
    BlockDevice *device;
} Ext2_FS;

extern FilesystemDriver ext2_driver;

/* super.c */
int ext2_read_bytes(Ext2_FS *fs, uint64_t byte_offset, uint64_t length, void *buffer);
int ext2_write_bytes(Ext2_FS *fs, uint64_t byte_offset, uint64_t length, void *buffer);

/* inode.c */
int ext2_read_inode(Ext2_FS *fs, uint32_t inode_num, Ext2_Inode *out_inode);
int ext2_write_inode(Ext2_FS *fs, uint32_t inode_num, Ext2_Inode *in_inode);

int ext2_read_block(Ext2_FS *fs, uint32_t block_num, void *buffer);
int ext2_write_block(Ext2_FS *fs, uint32_t block_num, void *buffer);

int ext2_read_inode_block(Ext2_FS *fs, Ext2_Inode *inode, uint8_t logical_index, void *buffer);
int ext2_write_inode_block(Ext2_FS *fs, Ext2_Inode *inode, uint8_t logical_index, void *buffer);

int ext2_read_dir(Ext2_FS *fs, Ext2_Inode *dir_inode, uint32_t *inode_offset, void *dir_entry);

int32_t ext2_read(Ext2_FS *fs, uint32_t inode_num, uint32_t offset, void *buffer, uint32_t count);
int32_t ext2_write(Ext2_FS *fs, uint32_t inode_num, uint32_t offset, const void *buffer, uint32_t size);

/* alloc.c */
int ext2_allocate_block(Ext2_FS *fs, uint32_t *out_block_num);
int ext2_free_block(Ext2_FS *fs, uint32_t block_num);

int ext2_allocate_inode(Ext2_FS *fs, uint32_t *out_inode_num);
int ext2_free_inode(Ext2_FS *fs, uint32_t inode_num);

/* namei.c */
uint32_t ext2_dir_lookup(Ext2_FS *fs, Ext2_Inode *dir_inode, const char *name);

int ext2_add_dir_entry(Ext2_FS *fs, uint32_t dir_inode_num, const char *name, uint32_t inode_num, Ext2_DIR_ENTRY_TYPE file_type);
int ext2_remove_dir_entry(Ext2_FS *fs, uint32_t dir_inode_num, const char *name);

int ext2_create(Ext2_FS *fs, uint32_t parent_dir_num, const char *name, Ext2_Inode *out_node);
int ext2_mkdir(Ext2_FS *fs, uint32_t parent_dir_num, const char *name);
int ext2_unlink(Ext2_FS *fs, uint32_t parent_dir_num, const char *name);
int ext2_rmdir(Ext2_FS *fs, uint32_t parent_dir_num, const char *name);
int ext2_rename(Ext2_FS *fs, uint32_t old_parent_inode, const char *old_name, uint32_t new_parent_inode, const char *new_name);


int vfs_ext2_init(VFS_Mount *mount, VFS_Node *out_root);

/* namespace_ops.c */
bool vfs_ext2_lookup_one(VFS_Node *dir, const char *name, VFS_Node *out_node);
int vfs_ext2_list_dir(VFS_Node *dir, Callback callback);
int vfs_ext2_create(VFS_Node *parent, const char *name, VFS_Node *out_node);
int vfs_ext2_mkdir(VFS_Node *parent, const char *name);
int vfs_ext2_unlink(VFS_Node *parent, const char *name);
int vfs_ext2_rmdir(VFS_Node *parent, const char *name);
int vfs_ext2_rename(VFS_Node *old_parent, const char *old_name, VFS_Node *new_parent, const char *new_name);

/* file_ops.c */
int64_t vfs_ext2_read(VFS_Node *node, uint32_t offset, void *buffer, uint32_t count);
int64_t vfs_ext2_write(VFS_Node *node, uint32_t offset, const void *buffer, uint32_t count);


#endif
