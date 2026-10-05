#ifndef VFS_SYSCALLS_H
#define VFS_SYSCALLS_H

#include <stdint.h>

int sys_read(uint32_t fd, void *buffer, uint32_t count);
int sys_write(uint32_t fd, const void *buffer, uint32_t count);


#endif
