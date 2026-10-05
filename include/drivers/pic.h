#ifndef PIC_H
#define PIC_H
#include <stdint.h>
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);


#endif
