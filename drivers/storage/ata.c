#include <stdint.h>
#include <stdlib.h>
#include "kernel/includes/main/cpu/io.h"
#include "../includes/storage/ata.h"
#include <time/ktime.h>

uint8_t ata_status = 0;

uint8_t ata_error  = 0;

void ata_write_48bit_lba(uint16_t io_base, uint64_t lba, uint16_t sector_count) {
    outb(io_base + ATA_REG_SECCOUNT, (sector_count >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_LOW,  (lba >> 24) & 0xFF);
    outb(io_base + ATA_REG_LBA_MID,  (lba >> 32) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 40) & 0xFF);

    outb(io_base + ATA_REG_SECCOUNT, sector_count & 0xFF);
    outb(io_base + ATA_REG_LBA_LOW,  lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
}

int ata_read_sector_28bit(uint32_t lba, uint8_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    if (lba >= (1UL << 28)) return -3;

    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= ((lba >> 24) & 0x0F);
    
    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    outb(io_base + ATA_REG_SECCOUNT, sector_count);
    outb(io_base + ATA_REG_LBA_LOW,  lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    uint8_t *buf = (uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = inw(io_base + ATA_REG_DATA);
            buf[s * sector_size_bytes + i] = word & 0xFF;
            buf[s * sector_size_bytes + i + 1] = word >> 8;
        }
    }
    return 0;
}

int ata_read_sector_48bit(uint64_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= 0x40;

    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);

    ata_write_48bit_lba(io_base, lba, sector_count);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS_EXT);

    uint8_t *buf = (uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = inw(io_base + ATA_REG_DATA);
            buf[s * sector_size_bytes + i] = word & 0xFF;
            buf[s * sector_size_bytes + i + 1] = word >> 8;
        }
    }
    return 0;
}

int ata_read_sector_28bit_manual(uint32_t lba, uint8_t sector_count, void *buffer, uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes) {
    if (lba >= (1UL << 28)) return -3;

    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= ((lba >> 24) & 0x0F);
    io_base = base_pmio;

    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    outb(io_base + ATA_REG_SECCOUNT, sector_count);
    outb(io_base + ATA_REG_LBA_LOW,  lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    uint8_t *buf = (uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = inw(io_base + ATA_REG_DATA);
            buf[s * sector_size_bytes + i] = word & 0xFF;
            buf[s * sector_size_bytes + i + 1] = word >> 8;
        }
    }
    return 0;
}

int ata_read_sector_48bit_manual(uint64_t lba, uint16_t sector_count, void *buffer, uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= 0x40;
    io_base = base_pmio;

    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);

    ata_write_48bit_lba(io_base, lba, sector_count);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS_EXT);

    uint8_t *buf = (uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = inw(io_base + ATA_REG_DATA);
            buf[s * sector_size_bytes + i] = word & 0xFF;
            buf[s * sector_size_bytes + i + 1] = word >> 8;
        }
    }
    return 0;
}

int ata_read_sector_auto(uint64_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    if ((lba < (1UL << 28)) && (sector_count <= 255)) {
        return ata_read_sector_28bit((uint32_t)lba, (uint8_t)sector_count, buffer, drive, sector_size_bytes);
    } else {
        if ((lba+sector_count) < (1UL << 28)) {
            for (uint32_t lba_reading=0; lba_reading<sector_count; lba_reading += 255) {
                uint16_t sector_count_reading = sector_count - lba_reading;
                if (sector_count_reading > 255) sector_count_reading = 255;
                int res = ata_read_sector_28bit((uint32_t)(lba + lba_reading), (uint8_t)sector_count_reading, (uint8_t *)buffer + lba_reading * sector_size_bytes, drive, sector_size_bytes);
                if (res != 0) return res;
            }
        }
        return ata_read_sector_48bit(lba, sector_count, buffer, drive, sector_size_bytes);
    }
}

int ata_read_sector_semiauto(uint64_t lba, uint16_t sector_count, void *buffer, uint16_t base_pmio, uint8_t drive, uint16_t sector_size_bytes) {
    if ((lba < (1UL << 28)) && (sector_count <= 255)) {
        return ata_read_sector_28bit_manual((uint32_t)lba, (uint8_t)sector_count, buffer, base_pmio, drive, sector_size_bytes);
    } else {
        if ((lba+sector_count) < (1UL << 28)) {
            for (uint32_t lba_reading=0; lba_reading<sector_count; lba_reading += 255) {
                uint16_t sector_count_reading = sector_count - lba_reading;
                if (sector_count_reading > 255) sector_count_reading = 255;
                int res = ata_read_sector_28bit_manual((uint32_t)(lba + lba_reading), (uint8_t)sector_count_reading, (uint8_t *)buffer + lba_reading * sector_size_bytes, base_pmio, drive, sector_size_bytes);
                if (res != 0) return res;
            }
        }
        return ata_read_sector_48bit_manual(lba, sector_count, buffer, base_pmio, drive, sector_size_bytes);
    }
}

int ata_write_sector_28bit(uint32_t lba, uint8_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    if (lba >= (1UL << 28)) return -3;

    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= ((lba >> 24) & 0x0F);

    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    outb(io_base + ATA_REG_SECCOUNT, sector_count);
    outb(io_base + ATA_REG_LBA_LOW,  lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint8_t *buf = (const uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = buf[s * sector_size_bytes + i] |
                            (buf[s * sector_size_bytes + i + 1] << 8);
            outw(io_base + ATA_REG_DATA, word);
        }
    }

    return ata_flush_cache(io_base, drive_head);
}

int ata_write_sector_48bit(uint64_t lba, uint16_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    drive_head |= 0x40;

    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);

    ata_write_48bit_lba(io_base, lba, sector_count);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS_EXT);

    const uint8_t *buf = (const uint8_t *)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -2;
        for (int i = 0; i < sector_size_bytes; i += 2) {
            uint16_t word = buf[s * sector_size_bytes + i] |
                            (buf[s * sector_size_bytes + i + 1] << 8);
            outw(io_base + ATA_REG_DATA, word);
        }
    }

    return ata_flush_cache_ext(io_base, drive_head);
}

int ata_write_sector_auto(uint64_t lba, uint16_t sector_count, const void *buffer, uint8_t drive, uint16_t sector_size_bytes) {
    if ((lba < (1UL << 28)) && (sector_count <= 255)) {
        return ata_write_sector_28bit((uint32_t)lba, (uint8_t)sector_count, buffer, drive, sector_size_bytes);
    } else {
        return ata_write_sector_48bit(lba, sector_count, buffer, drive, sector_size_bytes);
    }
}

int ata_flush_cache(uint16_t io_base, uint8_t drive_head) {
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_wait_ready(io_base, 0);
}

int ata_flush_cache_ext(uint16_t io_base, uint8_t drive_head) {
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
    return ata_wait_ready(io_base, 0);
}

int identify_ata(uint8_t drive, uint16_t identify_data[256]) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);

    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (ata_wait_ready(io_base, 1) != 0) return -1;

    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }

    return (identify_data[0] == 0) ? -2 : 0;
}

int identify_ata_manual(uint8_t drive, uint16_t identify_data[256], uint16_t pmio_base) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    io_base = pmio_base;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    for (int i = 0; i < 4; i++) inb(io_base + ATA_REG_STATUS);

    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (ata_wait_ready(io_base, 1) != 0) return -1;

    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }

    return (identify_data[0] == 0) ? -2 : 0;
}
