#include <memory/pmm.h>
#include <libs/memory.h>

#define PD_BASE_ADDRESS ((uint32_t*)0xFFFFF000)
#define PT_BASE_ADDRESS ((uint32_t*)0xFFC00000)

uint32_t *pageDirectory;


bool boot_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags);

void Paging_Init(){
    pageDirectory =  (uint32_t *)PMM_alloc_frame();
    memset(pageDirectory, 0, BLOCK_SIZE);
    pageDirectory[1023] = (uint32_t)pageDirectory | 0x03;
    uintptr_t end = getbitmapEnd();
    for (uintptr_t i = 0; i < end; i = i + BLOCK_SIZE){
        boot_map_page(i, i, 0x03);
    }
    asm volatile ("mov %0, %%cr3" : : "r"((uint32_t)pageDirectory));
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31);
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));
}



bool boot_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags){

    if ((physical_address & 0xFFF) != 0) return false;

    uint16_t directory_index = virtual_address >> 22;
    uint16_t table_index = (virtual_address >> 12) & 0x3FF;

    if (!(pageDirectory[directory_index] & 0x1)){
        uintptr_t newPageTable_address = PMM_alloc_frame();
        if(newPageTable_address == 0) return false;
        memset((uintptr_t *)newPageTable_address, 0, BLOCK_SIZE);
        pageDirectory[directory_index] = newPageTable_address | 0x3;
    }
    uint32_t *pageTable = (uint32_t *)(pageDirectory[directory_index] & 0xFFFFF000);
    pageTable[table_index] = physical_address | flags | 0x1;
    return true;
}

bool map_page(uintptr_t virtual_address, uintptr_t physical_address, uint32_t flags){

    if ((physical_address & 0xFFF) != 0) return false;

    uint32_t directory_index = virtual_address >> 22;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    
    if ((PD_BASE_ADDRESS[directory_index] & 0x1) == 0){
        uintptr_t newPageTable_address = PMM_alloc_frame();
        if(newPageTable_address == 0) return false;
        PD_BASE_ADDRESS[directory_index] = newPageTable_address | 0x3;

        // zero out the page table
        uint32_t* page_table = (uint32_t *)((uintptr_t)PT_BASE_ADDRESS + (directory_index * 0x1000)); // virtual memory
        memset(page_table, 0, BLOCK_SIZE); 
    }

    uint32_t* page_table = (uint32_t*)((uint32_t)PT_BASE_ADDRESS + (directory_index * 0x1000));
    page_table[table_index] = physical_address | (flags & 0xFFF) | 0x1;

    asm volatile ("invlpg (%0)" :: "r"(virtual_address) : "memory");

    return true;
}
