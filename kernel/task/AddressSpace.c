#include <task/task.h>

#define MAX_THREAD_SLOTS 1024


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
