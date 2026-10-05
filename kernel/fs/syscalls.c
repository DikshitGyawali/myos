#include <fs/vfs.h>
#include <task/task.h>
#include <memory/boot_info.h>
#include <memory/paging.h>
#include <memory/heap.h>
#include <error.h>
#include <stdbool.h>
#include <stddef.h>

#define PATH_MAX 4096

#define O_ACCMODE   3
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2

static bool access_ok(const void *buf, uint32_t count){
    uintptr_t start = (uintptr_t)buf;

    if (count == 0) return true;
    if (start >= (uintptr_t)&HIGHER_HALF) return false;
    if (count > (uintptr_t)&HIGHER_HALF - start) return false;

    uintptr_t page = start & ~(BLOCK_SIZE - 1);
    uintptr_t last_page = (start + count - 1) & ~(BLOCK_SIZE - 1);

    for (; page <= last_page; page += BLOCK_SIZE){
        if (!user_page_present(page)) return false; // confirm this is actually its "not mapped" signal
    }
    return true;
}

int sys_read(uint32_t fd, void *buffer, uint32_t count){
    if (fd > 15) return -EBADF;
    if(!access_ok(buffer, count)) return -EFAULT;

    OpenFile *file = running->addr_space->fds[fd];
    if (file == NULL) return -EBADF;
    if ((file->flags & O_ACCMODE) == O_WRONLY) return -EBADF;

    int64_t r = file->node->mount->driver->file_ops.read(file->node, file->offset, buffer, count);
    if (r < 0) return r;

    file->offset += r;
    return r;
}

int sys_write(uint32_t fd, const void *buffer, uint32_t count){
    if (fd > 15) return -EBADF;
    if(!access_ok(buffer, count)) return -EFAULT;

    OpenFile *file = running->addr_space->fds[fd];
    if (file == NULL) return -EBADF;
    if ((file->flags & O_ACCMODE) == O_RDONLY) return -EBADF;

    int64_t r = file->node->mount->driver->file_ops.write(file->node, file->offset, buffer, count);
    if (r < 0) return r;

    file->offset += r;
    return r;
}


int32_t user_string_ok(const char *str, uint32_t max_len){
    uintptr_t last_page = 0;
    bool checked_any = false;

    for (uint32_t i = 0; i < max_len; i++){
        uintptr_t page = ((uintptr_t)str + i) & ~(BLOCK_SIZE - 1);

        if (!checked_any || page != last_page){
            if (!user_page_present(page)) return -EFAULT;
            last_page = page;
            checked_any = true;
        }

        if (str[i] == '\0') return (int32_t)i;
    }

    return -ENAMETOOLONG;
}

int sys_open(char *path, uint32_t flags){
    int len = user_string_ok(path, PATH_MAX);
    if (len < 0) return len;

    VFS_Node *file_node = (VFS_Node *)kmalloc(sizeof(VFS_Node));
    if (file_node == NULL )return -ENOMEM;

    int r = vfs_resolve_path(vfs_root, path, file_node);
    if(r < 0) {kfree(file_node); return r;}

    if (file_node->is_directory && (flags & O_ACCMODE) != O_RDONLY) {kfree(file_node); return -EISDIR;}

    OpenFile *file = (OpenFile *)kmalloc(sizeof(OpenFile));
    if (file == NULL) {kfree(file_node); return -ENOMEM;}
    file->flags = flags;
    file->node = file_node;
    file->offset = 0;
    file->refcount = 1;

    for (uint8_t i = 0; i < 16; ++i){
        if(running->addr_space->fds[i] != NULL) continue;
        
        running->addr_space->fds[i] = file;
        return i;
    }
    return -EMFILE;
}

int sys_close(uint32_t fd){
    if (fd > 15) return -EBADF;
    OpenFile *file = running->addr_space->fds[fd];
    if(file == NULL) return -EBADF;

    file->refcount--;
    if(file->refcount == 0) {kfree(file->node); kfree(file);}

    running->addr_space->fds[fd] = NULL;

    return 0;
}
