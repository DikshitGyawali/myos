#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <memory/heap.h>
#include <task/task.h>

__attribute__((noreturn))
void kernel_main(){
    clear_screen();
    HAL_init();
    heap_init();

    shell_init();
    multitask_init();
    
    while (1) {
        __asm__("hlt");
    }
}
