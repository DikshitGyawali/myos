#include <task/task.h>
#include <memory/heap.h>
#include <memory/pmm.h>
#include <memory/paging.h>
#include <IO/screen.h>
#include <libs/mem_utils.h>
#include <arch/i686/gdt.h>
#include <arch/i686/isr.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

extern uint8_t _user_wrapper_start[];
extern uint8_t boot_stack_bottom[];
extern void user_task_wrapper();

#define USER_WRAPPER_PDE_IDX (((uintptr_t)&_user_wrapper_start) >> 22)

static TCB *head; //linklist that has the next task
TCB *running;
static TCB *swapperTCB;

static TCB *task_to_be_terminated = 0;
bool need_resched = false;

bool alloc_thread_slot(AddressSpace *as, uint32_t *out_slot);
void free_thread_slot(AddressSpace *as, uint32_t slot);

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
    kprintf("Killing task %d\n", task_to_kill->task_id);

    PMM_free_frame(task_to_kill->kstack_low_phys);
    free_thread_slot(task_to_kill->addr_space, task_to_kill->thread_slot);
    uint32_t vaddr = user_region_top(task_to_kill->thread_slot) - BLOCK_SIZE;
    for (uint32_t i = 0; i < task_to_kill->ustack_committed_pages; ++i){
        uintptr_t phys_addr = get_physical_address(vaddr);
        unmap_page(vaddr);
        PMM_free_frame(phys_addr);
        vaddr -= BLOCK_SIZE;
    }

    task_to_kill->addr_space->refcount--;

    if (task_to_kill->addr_space->refcount == 0){
        PMM_free_frame(task_to_kill->addr_space->pt_phys);
        PMM_free_frame(task_to_kill->addr_space->pd_phys);
        kfree(task_to_kill->addr_space);
        
    }
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
    AddressSpace *addr_space = NULL;

    swapperTCB = (TCB *)kmalloc(sizeof(TCB));
    if (swapperTCB == NULL) goto clean_up;

    addr_space = (AddressSpace *)kmalloc(sizeof(AddressSpace));
    if (addr_space == NULL) goto clean_up;

    memset(addr_space->thread_slot_bitmap, 0, 128);
    set_thread_slot(addr_space, 0);

    addr_space->pd_phys = get_physical_address((uintptr_t)PD_BASE_ADDRESS);
    addr_space->pt_phys = 0;
    addr_space->refcount = 1;
    
    swapperTCB->task_id = 0;
    swapperTCB->kstack_low_phys = (uintptr_t)&boot_stack_bottom - (uintptr_t)&HIGHER_HALF; // from trampoline asm
    swapperTCB->entry = swapper;
    swapperTCB->state = RUNNING;
    swapperTCB->addr_space = addr_space;
    swapperTCB->thread_slot = 0;
    swapperTCB->is_user = false;
    swapperTCB->ustack_committed_pages = 0;
    swapperTCB->next = NULL;
    __asm__ volatile ("mov %%esp, %0": "=r"(swapperTCB->saved_esp));
    running = swapperTCB;
    swapper();
    
clean_up:
    if (swapperTCB != NULL) kfree(swapperTCB);
    if (addr_space != NULL) kfree(addr_space);
    panic("SETUP_TASK0 FAILED, ABORTING...", __FILE__, __LINE__);
}

uint32_t __attribute__((cdecl)) switch_task(){
    ASSERT(running != NULL);

    TCB *old = running;
    TCB *next = head;
    while (task_to_be_terminated) {
        TCB *next = task_to_be_terminated->next;
        kill_task(task_to_be_terminated);
        task_to_be_terminated = next;
    }
    if (next == NULL){
        if (old->state == TERMINATED){
            swapperTCB->state = RUNNING;
            running = swapperTCB;
            g_TSS.esp0 = (uintptr_t)&HIGHER_HALF - swapperTCB->thread_slot * BLOCK_SIZE;
            return running->addr_space->pd_phys;
        }
        return 0;
    }
    
    head = head->next;
    
    if (old->state != TERMINATED){
        old->state = READY;
        if(old != swapperTCB) push_to_ready_queue(old);
    }

    next->state = RUNNING;
    running = next;
    g_TSS.esp0 = (uintptr_t)&HIGHER_HALF - next->thread_slot * BLOCK_SIZE;

    if (old->addr_space != next->addr_space) return running->addr_space->pd_phys;
    return 0;
}

__attribute__((noreturn))
static void task_wrapper(){
    running->entry();
    //kprintf("Task %d is no longer active\n", running->task_id);
    task_exit();
}

__attribute__((noreturn))
void task_exit(){ // called by syscall 0
    running->state = TERMINATED;
    running->next = NULL;

    TCB *it = task_to_be_terminated;
    if (it){
        while (it->next) it = it->next;
        it->next = running;
    }
    else task_to_be_terminated = running;

    while (true) __asm__ volatile ("int $0x81");
}


extern uint32_t __attribute__((cdecl)) get_eflags();

static uintptr_t* fill_stack(uintptr_t *ptr, uint32_t thread_slot, bool supervisor, TaskMain function){
    
    if (!supervisor){
        ptr = ptr - 1;
        *(ptr) = i686_GDT_USER_DATA_SEGMENT;
        ptr = ptr - 1;
        *(ptr) =  user_region_top(thread_slot);
    }

    ptr = ptr - 1; 
    *(ptr) = get_eflags()| (1 << 9);// eflags, force enable interrupts

    ptr = ptr - 1;
    if (supervisor) // CS
        *(ptr) = i686_GDT_CODE_SEGMENT;
    else 
        *(ptr) = i686_GDT_USER_CODE_SEGMENT;

    ptr = ptr - 1;
    *(ptr) = supervisor ? (uintptr_t)task_wrapper : (uintptr_t)user_task_wrapper; // eip



    for (int i = 0;i < 10; ++i){ //int_num, error_code, edi, esi, ebp, esp, ebx, edx, ecx
        ptr = ptr - 1;
        *(ptr) = (i == 2 && !supervisor) ? (uintptr_t)function : 0; // eax stores entry for user space, as userspace cannot access running
    }

    ptr = ptr - 1;
    if (supervisor) // DS
        *(ptr) = i686_GDT_DATA_SEGMENT;
    else 
        *(ptr) = i686_GDT_USER_DATA_SEGMENT;

    return ptr;
}


static uint32_t highest_unused_task_id = 1;

bool create_process(TaskMain function, bool supervisor){
    uintptr_t pd_phys = 0, kstack_pt_phys = 0, kstack_phys = 0;
    TCB *task = NULL;
    AddressSpace *addr_space = NULL;

    pd_phys = PMM_alloc_frame();
    if (pd_phys == 0) return false;

    kstack_pt_phys = PMM_alloc_frame();
    if (kstack_pt_phys == 0) goto clean_up;

    kstack_phys = PMM_alloc_frame();
    if (kstack_phys == 0) goto clean_up;

    task = (TCB *)kmalloc(sizeof(TCB));
    if (task == NULL) goto clean_up;

    addr_space = (AddressSpace *)kmalloc(sizeof(AddressSpace));
    if (addr_space == NULL) goto clean_up;

    memset(addr_space->thread_slot_bitmap, 0, 128);


    uint32_t *pd_virt = (uint32_t *)temp_map(pd_phys, 0);
    memset(pd_virt, 0, BLOCK_SIZE);
    for (uint32_t i = (uint32_t)&HIGHER_HALF >> 22; i < 1023; i++){
        pd_virt[i] = PD_BASE_ADDRESS[i];
    }
    pd_virt[USER_WRAPPER_PDE_IDX] = (PD_BASE_ADDRESS[USER_WRAPPER_PDE_IDX] & ~0xFFF) | 0x07;
    uint32_t trampoline_table_idx = ((uintptr_t)&_user_wrapper_start >> 12) & 0x3FF;
    uint32_t *trampoline_pt_virt = temp_map(PD_BASE_ADDRESS[USER_WRAPPER_PDE_IDX] & ~0xFFF, 1);
    trampoline_pt_virt[trampoline_table_idx] = (trampoline_pt_virt[trampoline_table_idx] & ~0xFFF) | 0x07;
    temp_unmap(1);

    pd_virt[1023] = (uint32_t)pd_phys | 0x03;

    uint32_t *stack_pt_virt = temp_map(kstack_pt_phys, 1);
    memset(stack_pt_virt, 0, BLOCK_SIZE);

    stack_pt_virt[1023 - 0] = kstack_phys | 0x03; // for KERNEL_STACK_VADDR
    pd_virt[KERNEL_STACK_PDE_IDX] = kstack_pt_phys | 0x03;
    temp_unmap(1);
    temp_unmap(0);

    
    set_thread_slot(addr_space, 0);
    addr_space->pd_phys = pd_phys;
    addr_space->pt_phys = kstack_pt_phys;
    addr_space->refcount = 1;
    
    task->task_id = highest_unused_task_id++;
    task->kstack_low_phys = (uintptr_t)kstack_phys;
    task->entry = function;
    task->state = READY;
    task->addr_space = addr_space;
    task->thread_slot = 0;
    task->ustack_committed_pages = 0;
    task->is_user = !supervisor;
    task->next = NULL;

    uint32_t *stack_virt = (uint32_t *)temp_map(kstack_phys, 0);
    
    uintptr_t *ptr = (uintptr_t *)((uintptr_t)stack_virt + BLOCK_SIZE);
    uintptr_t new_ptr = (uintptr_t)fill_stack(ptr, 0, supervisor, task->entry);
    task->saved_esp = ((uintptr_t)&HIGHER_HALF - task->thread_slot*BLOCK_SIZE) - (((uintptr_t)stack_virt + BLOCK_SIZE) - new_ptr);
    temp_unmap(0);
    push_to_ready_queue(task);
    return true;

clean_up:
    if (pd_phys != 0) PMM_free_frame(pd_phys);
    if (kstack_pt_phys != 0) PMM_free_frame(kstack_pt_phys);
    if (kstack_phys != 0) PMM_free_frame(kstack_phys);
    if (task != NULL) kfree(task);
    if (addr_space != NULL) kfree(addr_space);
    return false;
}

bool create_thread(TaskMain function, bool supervisor){
    ASSERT(running != NULL && running->addr_space != NULL);
    uintptr_t stack_phys = 0;
    TCB *task = NULL;

    stack_phys = PMM_alloc_frame();
    if (stack_phys == 0) goto clean_up;

    task = (TCB *)kmalloc(sizeof(TCB));
    if (task == NULL) goto clean_up;

    AddressSpace *addr_space = running->addr_space;
    if (!alloc_thread_slot(addr_space, &task->thread_slot)) goto clean_up;
    addr_space->refcount++;

    uint32_t *pd_virt = PD_BASE_ADDRESS;
    uint32_t *stack_pt_virt = temp_map(pd_virt[KERNEL_STACK_PDE_IDX] & ~0xFFF, 0);
    stack_pt_virt[1023 - task->thread_slot] = stack_phys | 0x03;
    temp_unmap(0);


    uint32_t *stack_virt = (uint32_t *)temp_map(stack_phys, 0);

    task->task_id = highest_unused_task_id++;
    task->kstack_low_phys = (uintptr_t)stack_phys;
    task->entry = function;
    task->state = READY;
    task->addr_space = addr_space;
    task->ustack_committed_pages = 0;
    task->is_user = !supervisor;
    task->next = NULL;

    uintptr_t *ptr = (uintptr_t *)((uintptr_t)stack_virt + BLOCK_SIZE);
    uintptr_t new_ptr = (uintptr_t)fill_stack(ptr, task->thread_slot, supervisor, task->entry);
    task->saved_esp = ((uintptr_t)&HIGHER_HALF - task->thread_slot*BLOCK_SIZE) - (((uintptr_t)stack_virt + BLOCK_SIZE) - new_ptr);
    temp_unmap(0);
    push_to_ready_queue(task);
    return true;

clean_up:
    if (stack_phys != 0) PMM_free_frame(stack_phys);
    if (task != NULL) kfree(task);
    return false;
}


void schedule(Registers* regs){ // 0x81, prabably need to change to allow task_wrapper-esque
    (void)regs;
    need_resched = true;
}

extern uint32_t __attribute((cdecl)) get_cr2();

void page_fault(Registers* regs){
    if (!running->is_user) goto fault;
    kprintf("\nDebug: inside pagefault\n");
    uintptr_t region_top = user_region_top(running->thread_slot);
    uintptr_t faulting_page = get_cr2() & ~0xFFF;
    uintptr_t current_floor = region_top - running->ustack_committed_pages * BLOCK_SIZE;
    kprintf("Debug: faulting_page=%x current_floor=%x \nregion_top=%x slot=%d committed=%d address=%x\n",
        faulting_page, current_floor, region_top, running->thread_slot, running->ustack_committed_pages, get_cr2());
    kprintf("0x%x == 0x%x\n", faulting_page, (current_floor - BLOCK_SIZE));
    if (faulting_page == (current_floor - BLOCK_SIZE) && running->ustack_committed_pages < USER_STACK_MAX_PAGES){
        uintptr_t frame =  PMM_alloc_frame();
        if (frame == 0) goto fault;
        if(!map_page(faulting_page, frame, 0x07)) goto fault;
        running->ustack_committed_pages++;
        return;
    }

fault:
    putchar('\n');
    printReg(regs);
    kprintf("CR2: %x", get_cr2());
    panic("KERNEL PANIC FROM PAGE FAULT HANDLER", __FILE__, __LINE__);
}


void multitask_init(){
    i686_ISR_RegisterHandler(0x81, schedule);
    i686_ISR_RegisterHandler(0xE, page_fault);
    setup_task0();
}
