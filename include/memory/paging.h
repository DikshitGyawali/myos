#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

void Paging_Init();
bool map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);

#endif
