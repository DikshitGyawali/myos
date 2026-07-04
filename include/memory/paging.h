#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

// void Paging_init();
bool map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
uintptr_t get_physical_address(uintptr_t virtual_address);
bool unmap_page(uintptr_t virtual_address);

#endif
