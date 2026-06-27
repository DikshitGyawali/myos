#ifndef MEMORY_LIBS_H
#define MEMORY_LIBS_H
#include <stdint.h>

void* memset(void* dest, int value, uint32_t count);
void* memcpy(void* dest, const void* src, uint32_t count);
int memcmp(const void* a, const void* b, uint32_t count);

#endif // MEMORY_LIBS_H
