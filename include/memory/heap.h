#ifndef HEAP_H
#define HEAP_H
#include <stddef.h>

void heap_init();
void* kmalloc(size_t payload_size);
void kfree(void *ptr);
void heap_map_print(char *args);
#endif
