#ifndef SCREEN_H
#define SCREEN_H
#include <stdarg.h>
#include <stdint.h>




void kprintf(const char* fmt, ...);
void putchar(char c);
void putchar_withcolor(char c, uint16_t color);
void print(const char *str);
void clear_screen();
void print_int(int num);
void print_hex(uint32_t num);
void panic(const char *message);

#endif // SCREEN_H
