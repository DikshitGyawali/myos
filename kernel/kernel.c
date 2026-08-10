#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <memory/heap.h>
#include <task/task.h>
#include <syscall/syscall.h>
#include <memory/boot_info.h>
extern TCB *running;

void short_task() {}


__attribute__((noreturn))
void kernel_main(){
    clear_screen();
    HAL_init();
    heap_init();
    syscall_isr_init();

    kprintf("end: 0x%x\n", &_kernel_virt_end);

    //shell_init();
    create_process(short_task, false);
    multitask_init();
    
    while (1) {
        __asm__("hlt");
    }
}
