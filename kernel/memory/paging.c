#include <memory/pmm.h>
#include <memory/paging.h>
#include <libs/mem_utils.h>
#include <IO/screen.h>


bool map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags){
    kprintf("Map Input: V=%x, P=%x, F=%x\n", virtual_address, physical_address, flags);
    if ((physical_address & 0xFFF) != 0) return false;

    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    bool new_table_created = false;
    uintptr_t newPageTable_address;
    if ((PD_BASE_ADDRESS[directory_index] & 0x1) == 0){
        newPageTable_address = PMM_alloc_frame();
        new_table_created = true;

        if(newPageTable_address == 0) return false;
        PD_BASE_ADDRESS[directory_index] = newPageTable_address | flags; // changed from 0x03 to flags

        asm volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3"::: "eax", "memory");

        // zero out the page table
        uint32_t* page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + (directory_index * 0x1000)); // virtual memory
        memset(page_table, 0, BLOCK_SIZE); 
    }

    uint32_t* page_table = (uint32_t*)((uint32_t)PT_BASE_ADDRESS + (directory_index * 0x1000));
    
    if (page_table[table_index] & 0x1){
        if (new_table_created)
        {
            PD_BASE_ADDRESS[directory_index] = 0;
            PMM_free_frame(newPageTable_address);
            asm volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3"::: "eax", "memory");
        }
        return false;
    }

    page_table[table_index] = physical_address | (flags & 0xFFF) | 0x1;

    asm volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3"::: "eax", "memory");

    return true;
}

uintptr_t get_physical_address(uintptr_t virtual_address){
    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    uint32_t offset = virtual_address & 0xFFF;

    if ((PD_BASE_ADDRESS[directory_index] & 0x1) == 0) return 0;

    uint32_t *page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + directory_index * BLOCK_SIZE);

    if ((page_table[table_index] & 0x1) == 0) return 0;

    return (page_table[table_index] & ~0xFFF) | offset;
}


bool unmap_page(uintptr_t virtual_address){
    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;

    if ((PD_BASE_ADDRESS[directory_index] & 0x1) == 0) return false;

    uint32_t *page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + directory_index * BLOCK_SIZE);

    if ((page_table[table_index] & 0x1) == 0) return false;

    page_table[table_index] = 0;

    asm volatile ("invlpg (%0)" :: "r"(virtual_address) : "memory");

    return true;
}


void *temp_map(uintptr_t physical_address, uint32_t table_index){
    uint32_t directory_index = TEMP_MAP_VADDR >> 22;
    uint32_t *page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + directory_index * BLOCK_SIZE);

    page_table[table_index] = (physical_address & ~0xFFF) | 0x3;

    uintptr_t vaddr = (directory_index << 22) | (table_index << 12);
    asm volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
    return (void *)vaddr;
}

void temp_unmap(uint32_t table_index){
    uint32_t directory_index = TEMP_MAP_VADDR >> 22;

    uint32_t *page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + directory_index * BLOCK_SIZE);
    page_table[table_index] = 0;

    asm volatile ("invlpg (%0)" :: "r"(TEMP_MAP_VADDR) : "memory");
}
