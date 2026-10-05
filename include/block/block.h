#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>
typedef struct BlockDevice BlockDevice;
struct BlockDevice {
    bool (*read_sector)(BlockDevice *dev, uint32_t sector_num, void *buffer);
    bool (*write_sector)(BlockDevice *dev, uint32_t sector_num, void *buffer);
    void *dev_data;
};




#endif
