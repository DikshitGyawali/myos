#include <drivers/keyboard.h>
#include <arch/i686/irq.h>
#include <IO/screen.h>
#include <arch/i686/port_io.h>
#include <stdint.h>

#define KEYBOARD_PORT 0x60
#define KB_BUF_SIZE 128


static const char keyboard_map[128] =
{
    0,
    27,
    '1','2','3','4','5','6','7','8','9','0',
    '-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p',
    '[',']',
    '\n',
    0,
    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',
    0,
    '\\',
    'z','x','c','v','b','n','m',
    ',', '.', '/',
    0,
    '*',
    0,
    ' ',
    // rest zero
};


char kb_buffer[KB_BUF_SIZE];
volatile int kb_head = 0;
volatile int kb_tail = 0;

char kb_getchar(){
    while (kb_tail == kb_head)
        __asm__("hlt");

    char c = kb_buffer[kb_tail++];
    kb_tail %= KB_BUF_SIZE;
    return c;
}


void Keyboard_Handler(Registers* regs){
    (void)regs;
    uint8_t scancode = i686_inb(KEYBOARD_PORT);

    if (scancode & 0x80)
        return;

    char c = keyboard_map[scancode];
    if (c == 0) return;
	kb_buffer[kb_head++] = c;
	kb_head %= KB_BUF_SIZE;
}


void Keyboard_Init()
{
    i686_IRQ_RegisterHandler(1, Keyboard_Handler);
}
