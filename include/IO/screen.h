#ifndef SCREEN_H
#define SCREEN_H
#include <stdarg.h>
#include <stdint.h>


typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN = 14,
    VGA_WHITE = 15,
} VGA;


void putchar(char c);
void putchar_withcolor(char c, uint16_t color);
void print(const char *str);
void clear_screen();
void print_int(int num);
void print_hex(uint32_t num);
void kprintf(const char* fmt, ...);
void panic(const char *message);

#endif // SCREEN_H
