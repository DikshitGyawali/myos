#include <IO/screen.h>
#include <stdint.h>
#include <stdarg.h>

#define MAX_X 80
#define MAX_Y 25
static uint16_t cursor_pos = 0;

void putchar_withcolor(char c, uint16_t color) {
    volatile char *video = (volatile char*)0xB8000;
    if (c == '\n') {
        video[cursor_pos] = c;
        video[cursor_pos] = color;
        cursor_pos += 160 - (cursor_pos % 160); // Move to the start of the next line
        return;
    }
    if (c == '\t') {
        cursor_pos += 8;
        return;
    }
    if (c == '\b'){
        if (cursor_pos == 0) return;
        cursor_pos -=2;
        video[cursor_pos] = (color << 8) | ' ';
        return;
    }
    video[cursor_pos++] = c;
    video[cursor_pos++] = color; 
}


void putchar(char c){
    putchar_withcolor(c, 0x07); // Light grey on black
}


void print(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putchar(str[i]);
    }
}

void clear_screen() {
    volatile char *video = (volatile char*)0xB8000;
    for (int i = 0; i < 80*25*2; i++) video[i] = 0;
    cursor_pos = 0;
}

void print_int(int num) {
    if (num == 0) {
        putchar('0');
        return;
    }

    char buffer[10];
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    for (int j = i - 1; j >= 0; j--) {
        putchar(buffer[j]);
    }
}
void print_hex(uint32_t num) {
    if (num == 0) {
        putchar('0');
        return;
    }

    char buffer[32];
    int i = 0;

    while (num > 0) {
        uint32_t digit = num % 16;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num /= 16;
    }
    
    for (int j = i - 1; j >= 0; j--) {
        putchar(buffer[j]);
    }
}


void kprintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt != '%'){
            putchar(*fmt++);
            continue;
        }
        fmt++;
        switch (*fmt){
        case 'c':
            char c = (char)va_arg(args, int);
            putchar(c);
            break;

        case 's':
            const char* s = va_arg(args, const char*);
            print(s);
            break;

        case 'd':
            int d = va_arg(args, int);
            print_int(d);
            break;

        case 'x':
            int32_t x = va_arg(args, int32_t);
            print_hex(x);
            break;
        case 'b':
            
            break;
        default:
            putchar('%');
            putchar(*fmt);
            break;
        }
        fmt++;
    }
}


void panic(const char *message) {
    kprintf("Error: %s\nSystem halted.", message);
    __asm__("cli");
    while (1) {
        __asm__("hlt");
    }
}
