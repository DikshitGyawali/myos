#include <arch/i686/irq.h>
#include <arch/i686/port_io.h>
#include <task/task.h>
#include <libs/mem_utils.h>
#include <block/block.h>


typedef enum{
    ATA_PRIMARY_DATA            = 0x1F0,
    ATA_PRIMARY_ERROR           = 0x1F1,
    ATA_PRIMARY_FEATURE         = 0x1F1,
    ATA_PRIMARY_SECCOUNT        = 0x1F2,
    ATA_PRIMARY_LBA_LO          = 0x1F3,
    ATA_PRIMARY_LBA_MID         = 0x1F4,
    ATA_PRIMARY_LBA_HI          = 0x1F5,
    ATA_PRIMARY_DRIVE_HEAD      = 0x1F6,
    ATA_PRIMARY_STATUS          = 0x1F7,
    ATA_PRIMARY_COMMAND         = 0x1F7,

} ATA_PRIMARY_REGS;

typedef enum{
    ATA_PRIMARY_ALT_STATUS      = 0x3F6,
    ATA_PRIMARY_DEVICE_CONTROL  = 0x3F6,
    ATA_PRIMARY_DRIVE_ADDR      = 0x3F7,

} ATA_PRIMARY_CONTROL_REGS;

typedef enum{
    // bits 0 - 3 is for 24 to 27 bits of LBA (or 0 to 3 bits of Head of CHS)
    ATA_DRIVE_MASTER            = 0x00, // bit 4 is for drive number (0-Master)
    ATA_DRIVE_SLAVE             = 0x10, // bit 4 is for drive number (1-Slave)
    ATA_DRIVE_CHS_MODE          = 0x00, // bit 6 is for mode (0-CHS mode)
    ATA_DRIVE_LBA_MODE          = 0x40, // bit 6 is for mode (1-LBA mode)
    ATA_DRIVE_OBSOLETE_BITS     = 0xA0, // bit 5 and 7 need to be set
} ATA_DRIVE_REG_BITS;

typedef enum{
    ATA_STATUS_ERR              = 0x01,
    ATA_STATUS_IDX              = 0x02,
    ATA_STATUS_CORR             = 0x04,
    ATA_STATUS_DRQ              = 0x08,
    ATA_STATUS_SRV              = 0x10,
    ATA_STATUS_DF               = 0x20,
    ATA_STATUS_RDY              = 0x40,
    ATA_STATUS_BSY              = 0x80,
} ATA_STATUS_REG_BITS;

typedef enum {
    ATA_CMD_READ_SECTORS  = 0x20,
    ATA_CMD_WRITE_SECTORS = 0x30,
    ATA_CMD_CACHE_FLUSH   = 0xE7,
    ATA_CMD_IDENTIFY      = 0xEC,
} ATA_COMMAND;

struct {
    uint16_t buffer[256];
    uint32_t  waiting_task_id;
    bool success;
    bool is_write;
    bool busy;
} ata_pending;


static bool ATA_read_sector(uint32_t sector_num, void *buffer, bool slave){
    if (ata_pending.busy) return false;
    i686_DisableInterrupts();

    ata_pending.waiting_task_id = running->task_id;
    ata_pending.is_write = false;
    ata_pending.busy = true;

    i686_outb(ATA_PRIMARY_DRIVE_HEAD, ATA_DRIVE_OBSOLETE_BITS | 
                                      (slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) | 
                                      ATA_DRIVE_LBA_MODE | 
                                      ((sector_num >> 24) & 0x0F));
    i686_outb(ATA_PRIMARY_SECCOUNT, 1);
    i686_outb(ATA_PRIMARY_LBA_LO, sector_num & 0xFF);
    i686_outb(ATA_PRIMARY_LBA_MID, (sector_num >> 8) & 0xFF);
    i686_outb(ATA_PRIMARY_LBA_HI, (sector_num >> 16) & 0xFF);
    i686_outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    block_running();
    bool ok = ata_pending.success;
    if (ok) memcpy(buffer, ata_pending.buffer, 512);
    i686_EnableInterrupts();
    return ok;
}

static bool ATA_write_sector(uint32_t sector_num, void *buffer, bool slave){
    if (ata_pending.busy) return false;
    i686_DisableInterrupts();
    memcpy(ata_pending.buffer, buffer, 512);
    ata_pending.waiting_task_id = running->task_id;
    ata_pending.is_write = true;
    ata_pending.busy = true;
    
    i686_outb(ATA_PRIMARY_DRIVE_HEAD, ATA_DRIVE_OBSOLETE_BITS | 
                                      (slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) | 
                                      ATA_DRIVE_LBA_MODE | 
                                      ((sector_num >> 24) & 0x0F));
    i686_outb(ATA_PRIMARY_SECCOUNT, 1);
    i686_outb(ATA_PRIMARY_LBA_LO, sector_num & 0xFF);
    i686_outb(ATA_PRIMARY_LBA_MID, (sector_num >> 8) & 0xFF);
    i686_outb(ATA_PRIMARY_LBA_HI, (sector_num >> 16) & 0xFF);
    i686_outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);

    /* Wait for DRQ before sending data */
    while (!(i686_inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ)) {
        if (i686_inb(ATA_PRIMARY_STATUS) & ATA_STATUS_ERR){
            return false;
        }
    }

    for (uint32_t i = 0; i < 256; i++)
        i686_outw(ATA_PRIMARY_DATA, ata_pending.buffer[i]);

    block_running();
    i686_EnableInterrupts();
    return ata_pending.success;
}

void ATA_ISR_Handler(Registers* regs){
    (void)regs;
    uint8_t status = i686_inb(ATA_PRIMARY_STATUS);
    if (status & ATA_STATUS_BSY){
        ata_pending.success = false;
        wake_task(ata_pending.waiting_task_id);
        return;
    }

    if ((status & ATA_STATUS_ERR) || (status & ATA_STATUS_DF)){
        ata_pending.success = false;
        wake_task(ata_pending.waiting_task_id);
        return;
    }

    if (!ata_pending.is_write) 
        for (uint16_t i = 0; i < 256; ++i)
            ata_pending.buffer[i] = i686_inw(ATA_PRIMARY_DATA);
    
    ata_pending.success = true;
    ata_pending.busy = false;
    wake_task(ata_pending.waiting_task_id);
}

static bool ata_dev_read(BlockDevice *dev, uint32_t sector_num, void *buffer){
    return ATA_read_sector(sector_num, buffer, *(bool*)dev->dev_data);
}
static bool ata_dev_write(BlockDevice *dev, uint32_t sector_num, void *buffer){
    return ATA_write_sector(sector_num, buffer, *(bool*)dev->dev_data);
}
static const bool ATA_MASTER = false, ATA_SLAVE = true;
BlockDevice ata_primary_master = { ata_dev_read, ata_dev_write, (void*)&ATA_MASTER };
BlockDevice ata_primary_slave  = { ata_dev_read, ata_dev_write, (void*)&ATA_SLAVE  };


void ATA_init(){
    i686_outb(ATA_PRIMARY_DEVICE_CONTROL, 0x00);
    i686_IRQ_RegisterHandler(14, ATA_ISR_Handler);
    // Identify first: TODO
}
