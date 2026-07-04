#include <memory/boot_info.h>
#include <memory/pmm.h>
#include <arch/i686/port_io.h>

#define PD_VIRT ((volatile uint32_t*)0xFFFFF000)

extern void kernel_main();
extern uint32_t saved_multiboot_ptr;

extern char _bss_start[];
extern char _bss_end[];

__attribute__((noreturn))
void kernel_high_entry(){
    // No C runtime startup here — zero .bss manually 
    for (char *p = _bss_start; p < _bss_end; ++p) *p = 0;

    PMM_init((MultibootInfo*)saved_multiboot_ptr);

    PD_VIRT[0] = 0; // remove identity mapping

    // flush TLB: reload CR3 with the same address
    asm volatile(
        "mov %%cr3, %%eax \n"
        "mov %%eax, %%cr3 \n"
        ::: "eax"
    );
    kernel_main();
    for(;;);
}
