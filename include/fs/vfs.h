#ifndef VFS_H
#define VFS_H

#include <block/block.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct VFS_Node VFS_Node;
typedef struct VFS_Mount VFS_Mount;
typedef struct MountEntry MountEntry;
typedef struct FilesystemDriver FilesystemDriver;

struct VFS_Node {
    uint64_t size;
    bool is_directory;
    void *fs_data;       // opaque — meaning defined entirely by whichever driver created it
    VFS_Mount *mount;
};

struct VFS_Mount {
    FilesystemDriver *driver;
    void *fs_data; // file system specific variables
    BlockDevice *device;
    VFS_Node *root;
};

struct MountEntry {
    VFS_Mount *covered_mount;
    void *covered_fs_data;
    VFS_Mount *target;
    MountEntry *next;
};


typedef void (*Callback)(const char *name, bool is_dir);

typedef struct {
    bool (*lookup_one)(VFS_Node *dir, const char *name, VFS_Node *out_node);
    int (*dir_list)(VFS_Node *dir, Callback callback);
    int (*create)(VFS_Node *parent, const char *name, VFS_Node *out_node);
    int (*mkdir)(VFS_Node *parent, const char *name);
    int (*unlink)(VFS_Node *parent, const char *name);
    int (*rmdir)(VFS_Node *parent, const char *name);
    int (*rename)(VFS_Node *old_parent, const char *old_name, VFS_Node *new_parent, const char *new_name);
} NamespaceOps;

typedef struct {
    int64_t (*read)(VFS_Node *node, uint32_t offset, void *buffer, uint32_t count);
    int64_t (*write)(VFS_Node *node, uint32_t offset, const void *buffer, uint32_t count);
    bool (*truncate)(VFS_Node *node, uint32_t new_size);
} FileOps;

struct FilesystemDriver{
    int (*init)(VFS_Mount *mount, VFS_Node *out_root);
    NamespaceOps namespace_ops;
    FileOps      file_ops;
    
};

extern FilesystemDriver ext2_driver;
extern VFS_Node *vfs_root;

int vfs_mount(FilesystemDriver *driver, BlockDevice *device, VFS_Node *mount_point);
int vfs_resolve_path(VFS_Node *start, const char *path, VFS_Node *out);
bool vfs_init();

#endif
