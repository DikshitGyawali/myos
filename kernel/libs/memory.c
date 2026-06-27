#include <libs/memory.h>
#include <stdint.h>


void* memset(void* dest, int value, uint32_t count){

    uint8_t* p = (uint8_t*)dest;

    while (count--){
        *p++ = (uint8_t)value;
    }

    return dest;
}


void* memcpy(void* dest, const void* src, uint32_t count){

    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    while (count--){
        *d++ = *s++;
    }

    return dest;
}


int memcmp(const void* a, const void* b, uint32_t count){

    const uint8_t* p1 = (const uint8_t*)a;
    const uint8_t* p2 = (const uint8_t*)b;

    while (count--){

        if (*p1 != *p2)
            return (*p1 < *p2) ? -1 : 1;

        p1++;
        p2++;
    }

    return 0;
}
