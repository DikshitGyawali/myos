#ifndef STRING_H
#define STRING_H
#include <stddef.h>

int strcmp(const char* a, const char* b);
size_t strlen(const char *str);
char* strcpy(char* __restrict dst, const char* __restrict src);
#endif
