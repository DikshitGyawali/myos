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


static TCB *ready_head; //linklist that has the next task
static TCB *waiting_head = 0; // linklist with only waiting task
static TCB *terminated_head = 0; // linklist with only terminated task

TCB *running;
static TCB *swapperTCB;

bool need_resched = false;


bool alloc_thread_slot(AddressSpace *as, uint32_t *out_slot){
    for (uint32_t slot = 0; slot < MAX_THREAD_SLOTS; slot++){
        if (!test_thread_slot(as, slot)){
            set_thread_slot(as, slot);
            *out_slot = slot;
            return true;
        }
    }
    return false;
}

void free_thread_slot(AddressSpace *as, uint32_t slot){
    clear_thread_slot(as, slot);
}


void push_to_ready_queue(TCB *task){
    if (task == NULL) return;
    if (ready_head == NULL) {
        ready_head = task;
        task->state = READY;
        task->next = NULL;
        return;
    }

    TCB *it = ready_head;
    while (it->next) it = it->next;

    it->next = task;
    task->state = READY;
    task->next = NULL;
}

static void free_user_mapped_pages(uintptr_t pd_phys){
    uint32_t *pd_virt = temp_map(pd_phys, 0);

    for (uint32_t pde_index = 0; pde_index < ((uintptr_t)&HIGHER_HALF >> 22); pde_index++){
        if (pde_index == KERNEL_STACK_PDE_IDX) continue;  // own dedicated page table, freed as a whole frame separately
        if ((pd_virt[pde_index] & 0x1) == 0) continue;

        uint32_t *pt_virt = temp_map(pd_virt[pde_index] & ~0xFFF, 1);
        for (uint32_t pte_index = 0; pte_index < 1024; pte_index++){
            if (pt_virt[pte_index] & 0x1){
                PMM_free_frame(pt_virt[pte_index] & ~0xFFF);
            }
        }
        temp_unmap(1);

        PMM_free_frame(pd_virt[pde_index] & ~0xFFF);
    }

    temp_unmap(0);
}

void kill_task(TCB *task_to_kill){
    kprintf("Killing task %d\n", task_to_kill->task_id);

    PMM_free_frame(task_to_kill->kstack_low_phys);
    PMM_free_frame(task_to_kill->kstack_high_phys);
    free_thread_slot(task_to_kill->addr_space, task_to_kill->thread_slot);

    uint32_t *kstack_pt_virt = temp_map(task_to_kill->addr_space->pt_phys, 1);
    kstack_pt_virt[KSTACK_PTE(task_to_kill->thread_slot, 0)] = 0;
    kstack_pt_virt[KSTACK_PTE(task_to_kill->thread_slot, 1)] = 0;
    temp_unmap(1);

    task_to_kill->addr_space->refcount--;

    if (task_to_kill->addr_space->refcount == 0){
        free_user_mapped_pages(task_to_kill->addr_space->pd_phys);
        PMM_free_frame(task_to_kill->addr_space->pt_phys);
        PMM_free_frame(task_to_kill->addr_space->pd_phys);
        kfree(task_to_kill->addr_space);
    }

    kfree(task_to_kill);
}

__attribute__((noreturn))
void cleaner(){
    while(true){
        while (terminated_head) {
            TCB *next = terminated_head->next;
            kill_task(terminated_head);
            terminated_head = next;
        }
        __asm__ volatile ("sti; hlt");
    }
}

__attribute__((noreturn))
void swapper(){
    while (true) {
        __asm__ volatile ("cli");
        if (ready_head) {
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
    memset(addr_space->fds, 0, sizeof(addr_space->fds));
    
    swapperTCB->task_id = 0;
    swapperTCB->kstack_low_phys = (uintptr_t)&boot_stack_bottom;
    swapperTCB->kstack_high_phys = (uintptr_t)&boot_stack_bottom + BLOCK_SIZE;
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
    TCB *next = ready_head;

    if (next == NULL){
        if (old->state == TERMINATED || old->state == WAITING){
            swapperTCB->state = RUNNING;
            running = swapperTCB;
            g_TSS.esp0 = KSTACK_TOP(swapperTCB->thread_slot);
            return running->addr_space->pd_phys;
        }
        return 0;
    }
    
    ready_head = ready_head->next;
    
    if (old->state != TERMINATED && old->state != WAITING){
        old->state = READY;
        if(old != swapperTCB) push_to_ready_queue(old);
    }

    next->state = RUNNING;
    running = next;
    g_TSS.esp0 = KSTACK_TOP(next->thread_slot);

    if (old->addr_space != next->addr_space) return running->addr_space->pd_phys;
    return 0;
}

__attribute__((noreturn))
static void task_wrapper(){
    running->entry();
    kprintf("Task %d is no longer active\n", running->task_id);
    task_exit();
}

__attribute__((noreturn))
void task_exit(){ // called by syscall 1
    running->state = TERMINATED;
    running->next = NULL;

    TCB *it = terminated_head;
    if (it){
        while (it->next) it = it->next;
        it->next = running;
    }
    else terminated_head = running;

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

TCB *create_process_TCB(TaskMain function, bool supervisor){
    uintptr_t pd_phys = 0, kstack_pt_phys = 0, kstack_low_phys = 0, kstack_high_phys = 0;
    TCB *task = NULL;
    AddressSpace *addr_space = NULL;

    pd_phys = PMM_alloc_frame();
    if (pd_phys == 0) return NULL;

    kstack_pt_phys = PMM_alloc_frame();
    if (kstack_pt_phys == 0) goto clean_up;

    kstack_low_phys = PMM_alloc_frame();
    if (kstack_low_phys == 0) goto clean_up;

    kstack_high_phys = PMM_alloc_frame();
    if (kstack_high_phys == 0) goto clean_up;

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

    stack_pt_virt[KSTACK_PTE(0, 1)] = kstack_low_phys | 0x03;
    stack_pt_virt[KSTACK_PTE(0, 0)] = kstack_high_phys | 0x03;

    pd_virt[KERNEL_STACK_PDE_IDX] = kstack_pt_phys | 0x03;
    temp_unmap(1);
    temp_unmap(0);

    
    set_thread_slot(addr_space, 0);
    addr_space->pd_phys = pd_phys;
    addr_space->pt_phys = kstack_pt_phys;
    addr_space->refcount = 1;
    memset(addr_space->fds, 0, sizeof(addr_space->fds));
    
    task->task_id = highest_unused_task_id++;
    task->kstack_low_phys = (uintptr_t)kstack_low_phys;
    task->kstack_high_phys = (uintptr_t)kstack_high_phys;
    task->entry = function;
    task->state = READY;
    task->addr_space = addr_space;
    task->thread_slot = 0;
    task->ustack_committed_pages = 0;
    task->is_user = !supervisor;
    task->next = NULL;

    uint32_t *stack_virt = (uint32_t *)temp_map(kstack_high_phys, 0);
    
    uintptr_t *ptr = (uintptr_t *)((uintptr_t)stack_virt + BLOCK_SIZE);
    uintptr_t new_ptr = (uintptr_t)fill_stack(ptr, 0, supervisor, task->entry);
    task->saved_esp = KSTACK_TOP(task->thread_slot) - (((uintptr_t)stack_virt + BLOCK_SIZE) - new_ptr);
    temp_unmap(0);
    return task;

clean_up:
    if (pd_phys != 0) PMM_free_frame(pd_phys);
    if (kstack_pt_phys != 0) PMM_free_frame(kstack_pt_phys);
    if (kstack_low_phys != 0) PMM_free_frame(kstack_low_phys);
    if (kstack_high_phys != 0) PMM_free_frame(kstack_high_phys);
    if (task != NULL) kfree(task);
    if (addr_space != NULL) kfree(addr_space);
    return NULL;
}

bool create_process(TaskMain function, bool supervisor){
    TCB *task = create_process_TCB(function, supervisor);
    if (task == NULL) return false;

    push_to_ready_queue(task);
    return true;
}

bool create_thread(TaskMain function, bool supervisor){
    ASSERT(running != NULL && running->addr_space != NULL);
    uintptr_t stack_low_phys = 0, stack_high_phys = 0;
    TCB *task = NULL;

    stack_low_phys = PMM_alloc_frame();
    if (stack_low_phys == 0) goto clean_up;

    stack_high_phys = PMM_alloc_frame();
    if (stack_high_phys == 0) goto clean_up;

    task = (TCB *)kmalloc(sizeof(TCB));
    if (task == NULL) goto clean_up;

    AddressSpace *addr_space = running->addr_space;
    if (!alloc_thread_slot(addr_space, &task->thread_slot)) goto clean_up;
    addr_space->refcount++;

    uint32_t *pd_virt = PD_BASE_ADDRESS;
    uint32_t *stack_pt_virt = temp_map(pd_virt[KERNEL_STACK_PDE_IDX] & ~0xFFF, 0);
    stack_pt_virt[KSTACK_PTE(task->thread_slot, 1)] = stack_low_phys | 0x03;
    stack_pt_virt[KSTACK_PTE(task->thread_slot, 0)] = stack_high_phys | 0x03;
    temp_unmap(0);


    uint32_t *stack_virt = (uint32_t *)temp_map(stack_high_phys, 0);

    task->task_id = highest_unused_task_id++;
    task->kstack_low_phys = (uintptr_t)stack_low_phys;
    task->kstack_high_phys = (uintptr_t)stack_high_phys;
    task->entry = function;
    task->state = READY;
    task->addr_space = addr_space;
    task->ustack_committed_pages = 0;
    task->is_user = !supervisor;
    task->next = NULL;

    uintptr_t *ptr = (uintptr_t *)((uintptr_t)stack_virt + BLOCK_SIZE);
    uintptr_t new_ptr = (uintptr_t)fill_stack(ptr, task->thread_slot, supervisor, task->entry);
    task->saved_esp = KSTACK_TOP(task->thread_slot) - (((uintptr_t)stack_virt + BLOCK_SIZE) - new_ptr);
    temp_unmap(0);
    push_to_ready_queue(task);
    return true;

clean_up:
    if (stack_low_phys != 0) PMM_free_frame(stack_low_phys);
    if (stack_high_phys != 0) PMM_free_frame(stack_high_phys);
    if (task != NULL) kfree(task);
    return false;
}

void block_running(){
    __asm__ volatile ("cli");
    running->state = WAITING;
    running->next = NULL;
    TCB *it = waiting_head;
    if (it){
        while (it->next) it = it->next;
        it->next = running;
    }
    else waiting_head = running;
    __asm__ volatile ("int $0x81");
}

bool wake_task(uint32_t task_id){
    TCB *it = waiting_head;
    TCB *temp = NULL;
    while (it){
        if (it->task_id == task_id){
            if (temp) temp->next = it->next;
            else waiting_head = it->next;
            it->next = NULL;
            push_to_ready_queue(it);
            return true;
        }
        temp = it;
        it = it->next;
    }
    return false;
}

void schedule(Registers* regs){ // 0x81, probably need to change to allow task_wrapper-esque
    (void)regs;
    need_resched = true;
}

extern uint32_t __attribute((cdecl)) get_cr2();

void page_fault(Registers* regs){
    if ((regs->cs & 3) != 3) goto fault;
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
    create_process(cleaner, true);
    setup_task0();
}
