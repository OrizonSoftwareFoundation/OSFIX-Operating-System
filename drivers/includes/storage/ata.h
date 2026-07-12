#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <stdint.h>
#include "../kernel/includes/main/cpu/io.h"

#define ATA_PRIMARY_IO     0x1F0
#define ATA_SECONDARY_IO   0x170
#define ATA_REG_DATA       0
#define ATA_REG_ERROR      1
#define ATA_REG_SECCOUNT   2
#define ATA_REG_LBA_LOW    3
#define ATA_REG_LBA_MID    4
#define ATA_REG_LBA_HIGH   5
#define ATA_REG_DRIVE_HEAD 6
#define ATA_REG_COMMAND    7
#define ATA_REG_STATUS     7
#define ATA_CMD_READ_SECTORS       0x20
#define ATA_CMD_READ_SECTORS_EXT   0x24
#define ATA_CMD_WRITE_SECTORS      0x30
#define ATA_CMD_WRITE_SECTORS_EXT  0x34
#define ATA_CMD_CACHE_FLUSH        0xE7
#define ATA_CMD_CACHE_FLUSH_EXT    0xEA
#define ATA_CMD_IDENTIFY           0xEC

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t ata_status;

extern uint8_t ata_error;

static const char *const ata_errors[] = {
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "NO DATA", "DISK BUSY",
};

static inline void ata_get_io_and_head(uint8_t drive, uint16_t *io_base, uint8_t *drive_head) {
    *io_base = (drive < 2) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    uint8_t master_slave = (drive & 1) ? 0x10 : 0x00;
    *drive_head = 0xE0 | master_slave;
}

static inline int ata_wait_ready(uint16_t io_base, int needs_drq) {
    for (int i = 0; i < 100000; i++) {
        ata_status = inb(io_base + ATA_REG_STATUS);
        if (ata_status & 0x01) {
            ata_error = inb(io_base + ATA_REG_ERROR);
            return -2;
        }
        if (!(ata_status & 0x80)) {
            if (!needs_drq || (ata_status & 0x08)) return 0;
        }
    }
    return -1;
}

void ata_write_48bit_lba(uint16_t io_base, uint64_t lba, uint16_t sector_count);

int ata_read_sector_28bit(uint32_t lba, uint8_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes);
int ata_read_sector_48bit(uint64_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes);
int ata_read_sector_auto(uint64_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes);

int ata_read_sector_28bit_manual(uint32_t lba, uint8_t sector_count, void *buffer,
                                 uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes);

int ata_read_sector_48bit_manual(uint64_t lba, uint16_t sector_count, void *buffer,
                                 uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes);

int ata_read_sector_semiauto(uint64_t lba, uint16_t sector_count, void *buffer,
                                 uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes);

int ata_write_sector_28bit(uint32_t lba, uint8_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes);
int ata_write_sector_48bit(uint64_t lba, uint16_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes);
int ata_write_sector_auto(uint64_t lba, uint16_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes);

int ata_flush_cache(uint16_t io_base, uint8_t drive_head);
int ata_flush_cache_ext(uint16_t io_base, uint8_t drive_head);

int identify_ata(uint8_t drive, uint16_t identify_data[256]);
int identify_ata_manual(uint8_t drive, uint16_t identify_data[256], uint16_t pmio_base);

#ifdef __cplusplus
}
#endif

#endif 
