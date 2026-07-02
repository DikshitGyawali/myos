#ifndef PMM_H
#define PMM_H
#include <memory/boot_info.h>
#include <stdbool.h>
#include <stdint.h>
void PMM_Init(const MultibootInfo* multiboot_info);
uintptr_t PMM_alloc_frame();
bool PMM_free_frame(uintptr_t addr);

uintptr_t getbitmapEnd();

#define BLOCK_SIZE 4096

#endif // PMM_H
