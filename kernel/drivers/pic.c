#include <arch/i686/port_io.h>
#include <stdint.h>


#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
// not complete/connected
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset){
    uint32_t address = (1u << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    i686_outl(PCI_CONFIG_ADDRESS, address);
    return i686_inl(PCI_CONFIG_DATA);
}

