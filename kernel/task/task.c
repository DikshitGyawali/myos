#include <task/task.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory/heap.h>
#include <IO/screen.h>
#include <assert.h>
#include <arch/i686/gdt.h>
#include <arch/i686/isr.h>


static TCB *head; //linklist that has the next task
TCB *running;
static TCB *swapperTCB;

static TCB *task_to_be_terminated;
bool need_resched = false; 

static void push_to_ready_queue(TCB *task){
    if (task == NULL) return;
    if (head == NULL) {
        head = task;
        task->state = READY;
        task->next = NULL;
        return;
    }

    TCB *it = head;
    while (it->next) it = it->next;

    it->next = task;
    task->state = READY;
    task->next = NULL;
}

static void kill_task(TCB *task_to_kill){
    kprintf("Killing task %d", task_to_kill->task_id);
    kfree((void *)task_to_kill->stack_low);
    kfree(task_to_kill);
}

__attribute__((noreturn))
void swapper(){
    while (1) {
        __asm__ volatile ("cli");
        while (task_to_be_terminated) {
            TCB *next = task_to_be_terminated->next;
            kill_task(task_to_be_terminated);
            task_to_be_terminated = next;
        }
        if (head) {
            __asm__ volatile ("int $0x81");
        } else {
            __asm__ volatile ("sti; hlt");
        }
    }
}

void setup_task0(){
    swapperTCB = (TCB *)kmalloc(sizeof(TCB));
    if (swapperTCB == NULL) return;

    swapperTCB->task_id = 0;
    swapperTCB->entry = swapper;
    swapperTCB->state = RUNNING;
    swapperTCB->stack_high = 0;
    swapperTCB->stack_low = 0;
    swapperTCB->next = NULL;
    __asm__ volatile ("mov %%esp, %0": "=r"(swapperTCB->saved_esp));
    running = swapperTCB;
    swapper();
}

void __attribute__((cdecl)) switch_task(){
    ASSERT(running != NULL);

    TCB *old = running;
    TCB *next = head;

    if (next == NULL){
        if (old->state == TERMINATED){
            swapperTCB->state = RUNNING;
            running = swapperTCB;
        }
        return;
    }
    
    head = head->next;
    
    if (old->state != TERMINATED){
        old->state = READY;
        if(old != swapperTCB) push_to_ready_queue(old);
    }

    next->state = RUNNING;
    running = next;
}

__attribute__((noreturn))
static void task_wrapper(){
    running->entry();
    kprintf("Task %d is no longer active\n", running->task_id);
    running->state = TERMINATED;
    running->next = NULL;

    TCB *it = task_to_be_terminated;
    if(it){
        while (it->next) it = it->next;
        it->next = running;
    }
    else task_to_be_terminated = running;

    while(true) __asm__ volatile ("int $0x81");
}


extern uint32_t __attribute__((cdecl)) get_eflags();

static uint32_t highest_unused_task_id = 1;

bool create_task(TaskMain function){
    TCB *task = (TCB *)kmalloc(sizeof(TCB));
    if (task == NULL) return false;
    char *stack = (char *)kmalloc(4080);
    if (stack == NULL){
        kfree(task);
        return false;
    }

    task->task_id = highest_unused_task_id++;
    task->stack_high = (uintptr_t)(stack + 4080);
    task->stack_low = (uintptr_t)stack;
    task->entry = function;
    task->state = READY;
    task->next = NULL;

    uintptr_t *ptr = (uintptr_t *)(task->stack_high);

    ptr = ptr - 1; 
    *(ptr) = get_eflags()| (1 << 9);// eflags, force enable interrupts

    ptr = ptr - 1;
    *(ptr) = i686_GDT_CODE_SEGMENT; // CS

    ptr = ptr - 1;
    *(ptr) = (uintptr_t)task_wrapper; // eip

    for (int i = 0;i < 10; ++i){ //edi, esi, ebp, the slot pusha reserves for esp, ebx, edx, ecx, eax, int_num, error_code
        ptr = ptr - 1;
        *(ptr) = 0;
    }

    ptr = ptr - 1;
    *(ptr) = i686_GDT_DATA_SEGMENT; // DS


    task->saved_esp = (uintptr_t)ptr;

    push_to_ready_queue(task);
    return true;
}

void schedule(Registers* regs){
    (void)regs;
    need_resched = true;
}

void multitask_init(){
    i686_ISR_RegisterHandler(0x81, schedule);
    setup_task0();
}
