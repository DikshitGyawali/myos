#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool (*Probe)();
    void (*Initialize)(uint8_t offsetPic1, uint8_t offsetPic2, bool autoEOI);
    void (*Disable)();
    void (*SendEOI)(uint8_t irq);
    void (*Mask)(int irq);
    void (*Unmask)(int irq);

} PICDriver;




#endif // PIC_H
