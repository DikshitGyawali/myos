#include <memory/heap.h>
#include <memory/pmm.h>
#include <memory/paging.h>
#include <IO/screen.h>
#include <shell/shell.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct {
    uint32_t size;    // WHOLE block: sizeof(header) + payload + sizeof(footer)
    uint8_t  is_free;
    uint8_t  _pad[3]; // pad to make 8 byte
} block_header_t;

typedef struct {
    uint32_t size;    // mirrors header.size exactly
    uint8_t  is_free;
    uint8_t  _pad[3]; // pad to make 8 byte
} block_footer_t;


static block_header_t *heap_start = NULL;
static uintptr_t heap_end;

#define MIN_BLOCK_SIZE (sizeof(block_header_t) + sizeof(block_footer_t))

#define GET_FOOTER(hdr) \
    ((block_footer_t *)((char*)(hdr) + (hdr)->size - sizeof(block_footer_t)))

#define NEXT_BLOCK(hdr) \
    ((block_header_t *)((char*)(hdr) + (hdr)->size))

#define PREV_FOOTER(hdr) \
    ((block_footer_t *)((char*)(hdr) - sizeof(block_footer_t)))

#define PREV_BLOCK(hdr) \
    ((block_header_t *)((char*)(hdr) - PREV_FOOTER(hdr)->size))


void heap_init(){
    heap_start = (block_header_t *)(firstunReserved() + (uintptr_t)&HIGHER_HALF);

    uintptr_t first_page =  PMM_alloc_frame();
    ASSERT(first_page != 0);
    ASSERT(map_page((uintptr_t)heap_start, first_page, 0x3));
    heap_end = (uintptr_t)heap_start + BLOCK_SIZE;

    heap_start->size = BLOCK_SIZE;
    heap_start->is_free = true;
    GET_FOOTER(heap_start)->is_free = true;
    GET_FOOTER(heap_start)->size = BLOCK_SIZE;
    shell_RegisterCommand((ShellCommand){"heap-map", heap_map_print});
}


block_header_t* split_block(block_header_t *blk, size_t needed_payload){

    ASSERT(GET_FOOTER(blk)->size == blk->size);
    ASSERT(GET_FOOTER(blk)->is_free == blk->is_free);
    ASSERT(blk->size > MIN_BLOCK_SIZE);
    ASSERT((blk->size & 7) == 0);

    if(blk->size <= MIN_BLOCK_SIZE * 2 + needed_payload){
        return blk;
    }
    uint32_t prev_block_size = blk->size;
    blk->size = MIN_BLOCK_SIZE + needed_payload;
    blk->is_free = false;
    GET_FOOTER(blk)->is_free = false;
    GET_FOOTER(blk)->size = MIN_BLOCK_SIZE + needed_payload;

    block_header_t *remaining_blk = NEXT_BLOCK(blk);

    remaining_blk->size = prev_block_size - blk->size;
    ASSERT(remaining_blk->size >= MIN_BLOCK_SIZE);
    remaining_blk->is_free = true;
    GET_FOOTER(remaining_blk)->size = prev_block_size - blk->size;
    GET_FOOTER(remaining_blk)->is_free = true;

    return blk;
}


block_header_t* find_free_block(size_t payload_size){
    block_header_t* block_it = heap_start;
    
    while((uintptr_t)block_it < heap_end){
        ASSERT(block_it->size > MIN_BLOCK_SIZE);
        ASSERT((block_it->size & 7) == 0);
        if (!block_it->is_free){
            block_it = NEXT_BLOCK(block_it);
            continue;
        }

        if((block_it->size - MIN_BLOCK_SIZE) < payload_size){
            block_it = NEXT_BLOCK(block_it);
            continue;
        }
        return split_block(block_it, payload_size);
    }
    return NULL;
}


bool heap_expand(size_t nbytes){
    uint32_t new_frames = (nbytes + BLOCK_SIZE - 1)/BLOCK_SIZE;
    block_header_t *last_block = PREV_BLOCK(heap_end);
    uintptr_t prev_heap_end = heap_end;

    uint32_t mapped = 0;
    for (; mapped < new_frames; mapped++){
        uintptr_t frame = PMM_alloc_frame();
        if (frame == 0) break;
        if (!map_page(heap_end, frame, 0x3)){
            PMM_free_frame(frame);
            break;
        }
        heap_end = heap_end + BLOCK_SIZE;
    }

    if (mapped != new_frames){
        while (mapped > 0){
            heap_end = heap_end - BLOCK_SIZE;
            mapped--;
            uintptr_t frame = get_physical_address(heap_end);
            if (frame != 0){
                unmap_page(heap_end);
                PMM_free_frame(frame);
            }
        }
        heap_end = prev_heap_end;
        return false;
    }

    if (last_block->is_free){
        last_block->size = last_block->size + BLOCK_SIZE * new_frames;
        last_block->is_free = true;
        GET_FOOTER(last_block)->size = last_block->size;
        GET_FOOTER(last_block)->is_free = true;
    }
    else{
        block_header_t *next_block = (block_header_t *)prev_heap_end;
        next_block->size = BLOCK_SIZE * new_frames;
        next_block->is_free = true;
        GET_FOOTER(next_block)->size = next_block->size;
        GET_FOOTER(next_block)->is_free = true;
    }
    return true;
}


void coalesce(block_header_t *blk){
    ASSERT(GET_FOOTER(blk)->size == blk->size);
    ASSERT(GET_FOOTER(blk)->is_free == blk->is_free);
    ASSERT(blk->size > MIN_BLOCK_SIZE);
    ASSERT((blk->size & 7) == 0);

    block_header_t temp_structure = (block_header_t){0, false, {0, 0, 0}};
    block_header_t *prev_blk;
    if (blk == heap_start) prev_blk = &temp_structure;
    else prev_blk = PREV_BLOCK(blk);

    block_header_t *next_blk;
    if (blk == PREV_BLOCK(heap_end)) next_blk = &temp_structure;
    else next_blk = NEXT_BLOCK(blk);

    if (!next_blk->is_free && !prev_blk->is_free) return;
    if (next_blk->is_free && !prev_blk->is_free){
        uint32_t total_size = blk->size + next_blk->size;
        blk->size = total_size;
        blk->is_free = true;

        GET_FOOTER(next_blk)->size = total_size;
        GET_FOOTER(next_blk)->is_free = true;
        return;
    }
    if (!next_blk->is_free && prev_blk->is_free){
        uint32_t total_size = prev_blk->size + blk->size;
        prev_blk->size = total_size;
        prev_blk->is_free = true;

        GET_FOOTER(blk)->size = total_size;
        GET_FOOTER(blk)->is_free = true;
        return;
    }
    if (next_blk->is_free && prev_blk->is_free){
        uint32_t total_size = prev_blk->size + blk->size + next_blk->size;
        prev_blk->size = total_size;
        prev_blk->is_free = true;

        GET_FOOTER(next_blk)->size = total_size;
        GET_FOOTER(next_blk)->is_free = true;
        return;
    }
}


void* kmalloc(size_t payload_size){
    payload_size = (payload_size + 7) & ~7;
    block_header_t * free_block = find_free_block(payload_size);
    if (free_block == NULL){
        if(!heap_expand(payload_size + MIN_BLOCK_SIZE))return NULL;
        free_block = find_free_block(payload_size);
        if (free_block == NULL) return NULL;
    }
    ASSERT(GET_FOOTER(free_block)->size == free_block->size);
    
    free_block->is_free = false;
    GET_FOOTER(free_block)->is_free = false;

    return (char*)free_block + sizeof(block_header_t);
}

void kfree(void *ptr){
    ASSERT(ptr != NULL);

    block_header_t *hdr = (block_header_t *)((char *)ptr - sizeof(block_header_t));
    ASSERT(!hdr->is_free);
    ASSERT(GET_FOOTER(hdr)->size == hdr->size);
    ASSERT(GET_FOOTER(hdr)->is_free == hdr->is_free);
    ASSERT((uintptr_t)hdr >= (uintptr_t)heap_start);
    ASSERT((uintptr_t)hdr < heap_end);

    hdr->is_free = true;
    GET_FOOTER(hdr)->is_free = true;
    coalesce(hdr); 
}


void heap_map_print(char *args){
    (void)args;
    block_header_t* block_it = heap_start;
    int i = 0;
    while((uintptr_t)block_it < heap_end){
        ASSERT(block_it->size > MIN_BLOCK_SIZE);
        ASSERT((block_it->size & 7) == 0);
        kprintf("%d> h(%x): s=%x, iF:%d\t", i,(uintptr_t)block_it ,block_it->size, block_it->is_free);
        kprintf("%d> f(%x): size=%x, isFree:%d\n", i,(uintptr_t)GET_FOOTER(block_it) ,GET_FOOTER(block_it)->size, GET_FOOTER(block_it)->is_free);
        block_it = NEXT_BLOCK(block_it);
        i++;
    }
}
