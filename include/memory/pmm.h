#ifndef PMM_H
#define PMM_H
#include <stdbool.h>
#include <stdint.h>
void PMM_Init();
uintptr_t PMM_alloc_frame();
bool PMM_free_frame(uintptr_t addr);

#endif // PMM_H
