#include <block/block.h>
#include <memory/heap.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
// not complete/completed
typedef struct {
    BlockDevice *underlying;
    uint32_t start_lba;
    uint32_t sector_count;
} PartitionInfo;

static bool partition_read(BlockDevice *dev, uint32_t sector_num, void *buffer){
    PartitionInfo *p = (PartitionInfo *)dev->dev_data;
    if (sector_num >= p->sector_count) return false;   // stay inside this partition's own span
    return p->underlying->read_sector(p->underlying, p->start_lba + sector_num, buffer);
}
static bool partition_write(BlockDevice *dev, uint32_t sector_num, void *buffer){
    PartitionInfo *p = (PartitionInfo *)dev->dev_data;
    if (sector_num >= p->sector_count) return false;
    return p->underlying->write_sector(p->underlying, p->start_lba + sector_num, buffer);
}

BlockDevice *make_partition_device(BlockDevice *underlying, uint32_t start_lba, uint32_t sector_count){
    PartitionInfo *info = kmalloc(sizeof(PartitionInfo));
    if (!info) return NULL;
    info->underlying = underlying; info->start_lba = start_lba; info->sector_count = sector_count;

    BlockDevice *dev = kmalloc(sizeof(BlockDevice));   // heap, deliberately — a stack-returned struct here
    if (!dev){ kfree(info); return NULL; }             // would dangle the instant this function returned,
    dev->read_sector = partition_read;                 // the same class of bug flagged with ephemeral
    dev->write_sector = partition_write;               // VFS_Node objects earlier
    dev->dev_data = info;
    return dev;
}
