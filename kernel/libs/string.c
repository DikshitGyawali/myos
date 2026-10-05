#include <libs/string.h>
#include <libs/mem_utils.h>
#include <stddef.h>


int strcmp(const char* a, const char* b){
    while (*a && *b){
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return *a - *b;
}

size_t strlen(const char *str){
    size_t length = 0;

    while (str[length] != '\0')
        length++;

    return length;
}

char* strcpy(char* __restrict dst, const char* __restrict src){
	const size_t length = strlen((const char*)src);
	memcpy(dst, src, length + 1);
	return dst;
}
