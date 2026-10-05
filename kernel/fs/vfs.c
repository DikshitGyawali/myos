#include <fs/ext2.h>
#include <fs/vfs.h>
#include <memory/heap.h>
#include <libs/mem_utils.h>
#include <error.h>
#include <panic.h>
#include <stddef.h>


VFS_Node *vfs_root;
static MountEntry *mount_list_head = NULL;
extern BlockDevice ata_primary_master;


bool vfs_init(){

    int r = vfs_mount(&ext2_driver, &ata_primary_master, NULL);
    if(r < 0) return false;
    else return true;
}


static bool vfs_register_mount_point(VFS_Node *mount_point, VFS_Mount *target){
    MountEntry *entry = kmalloc(sizeof(MountEntry));
    if (!entry) return false;
    entry->covered_mount = mount_point->mount;
    entry->covered_fs_data = mount_point->fs_data;
    entry->target = target;
    entry->next = mount_list_head;
    mount_list_head = entry;
    return true;
}

static VFS_Mount *check_mount_redirect(VFS_Node *node){
    for (MountEntry *e = mount_list_head; e; e = e->next)
        if (e->covered_mount == node->mount && e->covered_fs_data == node->fs_data)
            return e->target;
    return NULL;
}

int vfs_mount(FilesystemDriver *driver, BlockDevice *device, VFS_Node *mount_point){
    VFS_Mount *mount = kmalloc(sizeof(VFS_Mount));
    if (!mount) return -ENOMEM;
    mount->device = device;

    VFS_Node *new_root = kmalloc(sizeof(VFS_Node));
    if (!new_root){ kfree(mount); return -ENOMEM; }

    int r = driver->init(mount, new_root);
    if (r < 0){ kfree(mount); kfree(new_root); return r; }

    mount->driver = driver;
    mount->root = new_root;
    new_root->mount = mount;

    if (mount_point == NULL){
        vfs_root = new_root;
    } else {
        if (!mount_point->is_directory){ kfree(mount); kfree(new_root); return -ENOTDIR; }
        if (!vfs_register_mount_point(mount_point, mount)){ kfree(mount); kfree(new_root); return -ENOMEM; }
    }
    return 0;
}

int vfs_resolve_path(VFS_Node *start, const char *path, VFS_Node *out){
    VFS_Node current = (path[0] == '/') ? *vfs_root : *start;
    if (path[0] == '/') path++;

    while (*path != '\0'){
        while (*path == '/') path++;

        VFS_Mount *redirect = check_mount_redirect(&current);
        if (redirect != NULL) current = *redirect->root;

        if (!current.is_directory) return -ENOTDIR;

        const char *seg = path;
        while (*path && *path != '/') path++;
        uint32_t len = path - seg;
        char name[len + 1];
        memcpy(name, seg, len);
        name[len] = '\0';

        VFS_Node next;
        if (!current.mount->driver->namespace_ops.lookup_one(&current, name, &next)) return -ENOENT;
        current = next;
    }
    *out = current;
    return 0; 
}
