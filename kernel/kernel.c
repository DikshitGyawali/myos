#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <memory/heap.h>
#include <task/task.h>
#include <syscall/syscall.h>
#include <fs/vfs.h>
#include <elf/elf.h>


void boot_init(){
    if(!vfs_init()) kprintf("error intializing vfs\n");
    int r = elf_exec("/user.elf");
    kprintf("elf_exec errno (if any): %d\n",-r);
}

__attribute__((noreturn))
void kernel_main(){
    clear_screen();
    HAL_init();
    heap_init();
    syscall_isr_init();
    kprintf("end: 0x%x\n", &_kernel_virt_end);
    
    shell_init();
    if(!create_process(boot_init, true)) kprintf("creating failed\n");
    multitask_init();
    
    while (1) {
        __asm__("hlt");
    }
}
