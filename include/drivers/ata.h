#ifndef ATA_H
#define ATA_H

#include <block/block.h>
#include <stdint.h>
#include <stdbool.h>

void ATA_init();

extern BlockDevice ata_primary_master;
extern BlockDevice ata_primary_slave;

#endif
