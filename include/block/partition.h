#ifndef PARTITION_H
#define PARTITION_H
#include <block/block.h>
#include <stdint.h>
//not complete/connected
typedef struct {
    uint8_t  boot_flag;
    uint8_t  start_chs[3];   // legacy, safe to ignore
    uint8_t  type;           // 0x00 = unused slot, 0x83 = a real Linux-native partition
    uint8_t  end_chs[3];     // legacy, safe to ignore
    uint32_t start_lba;
    uint32_t sector_count;
} __attribute__((packed)) MBR_PartitionEntry;         // 16 bytes

typedef struct {
    uint8_t bootstrap[446];
    MBR_PartitionEntry partitions[4];
    uint16_t signature;       // must read 0xAA55
} __attribute__((packed)) MBR;                        // exactly 512 bytes — one sector, always sector 0

void discover_partitions(BlockDevice *dev);
#endif
