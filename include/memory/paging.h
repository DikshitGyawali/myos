#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

bool map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);
uintptr_t get_physical_address(uintptr_t virtual_address);
bool unmap_page(uintptr_t virtual_address);

#define PD_BASE_ADDRESS ((uint32_t*)0xFFFFF000)
#define PT_BASE_ADDRESS ((uint32_t*)0xFFC00000)


void *temp_map(uintptr_t physical_address, uint32_t table_index);
void temp_unmap(uint32_t table_index);
#define TEMP_MAP_VADDR 0xFFA00000 // see trampoline for why 1022 was used

#endif
