#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/includes/main/cpu/io.h"
#include <endian.h>
#include "../includes/storage/ata.h"
#include "../includes/storage/atapi.h"

uint8_t atapi_status = 0;

uint8_t atapi_error  = 0;

int atapi_read_sector(uint32_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint32_t sector_size) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 8);
    outb(io_base + ATA_REG_COMMAND, ATAPI_CMD_PACKET);

    if (ata_wait_ready(io_base, 1) != 0) {
        ata_status = inb(io_base + ATA_REG_STATUS);
        ata_error  = inb(io_base + ATA_REG_ERROR);
        return -2;
    }

    uint8_t packet[12] = {0};
    build_read10_atapi_cmd(packet, lba, sector_count);

    outsw(io_base + ATA_REG_DATA, packet, 6);

    uint16_t *buf = (uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -3;

        for (uint32_t i = 0; i < (sector_size / 2); i++) {
            buf[s * (sector_size / 2) + i] = inw(io_base + ATA_REG_DATA);
        }
    }

    return 0;
}

int atapi_read_sector_manual(uint32_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t pmio_base, uint32_t sector_size) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    io_base = pmio_base;
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 8);
    outb(io_base + ATA_REG_COMMAND, ATAPI_CMD_PACKET);

    if (ata_wait_ready(io_base, 1) != 0) {
        ata_status = inb(io_base + ATA_REG_STATUS);
        ata_error  = inb(io_base + ATA_REG_ERROR);
        return -2;
    }

    uint8_t packet[12] = {0};
    build_read10_atapi_cmd(packet, lba, sector_count);

    outsw(io_base + ATA_REG_DATA, packet, 6);

    uint16_t *buf = (uint16_t*)buffer;
    for (int s = 0; s < sector_count; s++) {
        if (ata_wait_ready(io_base, 1) != 0) return -3;

        for (uint32_t i = 0; i < (sector_size / 2); i++) {
            buf[s * (sector_size / 2) + i] = inw(io_base + ATA_REG_DATA);
        }
    }

    return 0;
}

static inline void ata_io_wait(uint16_t io_base) {
    for (int i = 0; i < 4; i++) {
        (void)inb(io_base + 0x206);
    }
}

int identify_atapi(uint8_t drive, uint16_t identify_data[256]) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_wait(io_base);

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);
    outb(io_base + ATA_REG_COMMAND, ATAPI_CMD_IDENTIFY);

    if (ata_wait_ready(io_base, 1) != 0) return -1;

    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }

    return 0;
}

int identify_atapi_manual(uint8_t drive, uint16_t identify_data[256], uint16_t pmio_base)  {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    io_base = pmio_base;

    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_wait(io_base);

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);
    outb(io_base + ATA_REG_COMMAND, ATAPI_CMD_IDENTIFY);

    if (ata_wait_ready(io_base, 1) != 0) return -1;

    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io_base + ATA_REG_DATA);
    }

    return 0;
}

int identify_atapi_useful(read10_capabillity_buffer_t *buffer, uint8_t drive) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW,  0);
    outb(io_base + ATA_REG_LBA_MID,  (sizeof(read10_capabillity_buffer_t)) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (sizeof(read10_capabillity_buffer_t)>>8) &0xFF);
    outb(io_base + ATA_REG_COMMAND,  ATAPI_CMD_PACKET);

    if (ata_wait_ready(io_base, 1) != 0) {
        ata_status = inb(io_base + ATA_REG_STATUS);
        ata_error  = inb(io_base + ATA_REG_ERROR);
        return -2;
    }

    uint8_t packet[12] = {0};
    build_read_capacity_atapi_cmd(packet);

    outsw(io_base + ATA_REG_DATA, packet, 6);

    if (ata_wait_ready(io_base, 1) != 0) {
        return -3;
    }

    uint16_t *buf = (uint16_t *)buffer;
    for (size_t i = 0; i < (size_t)sizeof(read10_capabillity_buffer_t) / 2; i++) {
        buf[i] = inw(io_base + ATA_REG_DATA);
    }

    read10_capabillity_buffer_t *buf32 = (read10_capabillity_buffer_t *)buffer;
    buf32->sector_size = be32toh(buf32->sector_size);
    buf32->last_lba = be32toh(buf32->last_lba);

    return 0;
}

int identify_atapi_useful_manual(read10_capabillity_buffer_t *buffer, uint8_t drive, uint16_t pmio_base) {
    uint16_t io_base;
    uint8_t drive_head;
    ata_get_io_and_head(drive, &io_base, &drive_head);
    io_base = pmio_base;
    outb(io_base + ATA_REG_DRIVE_HEAD, drive_head);
    if (ata_wait_ready(io_base, 0) != 0) return -1;

    outb(io_base + ATA_REG_FEATURES, 0);
    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW,  0);
    outb(io_base + ATA_REG_LBA_MID,  (uint8_t)((sizeof(read10_capabillity_buffer_t)) & 0xFF));
    outb(io_base + ATA_REG_LBA_HIGH, (uint8_t)((sizeof(read10_capabillity_buffer_t)>>8) &0xFF));
    outb(io_base + ATA_REG_COMMAND,  ATAPI_CMD_PACKET);

    if (ata_wait_ready(io_base, 1) != 0) {
        ata_status = inb(io_base + ATA_REG_STATUS);
        ata_error  = inb(io_base + ATA_REG_ERROR);
        return -2;
    }

    uint8_t packet[12] = {0};
    build_read_capacity_atapi_cmd(packet);

    outsw(io_base + ATA_REG_DATA, packet, 6);

    uint16_t *buf = (uint16_t *)buffer;
    for (size_t i = 0; i < (size_t)sizeof(read10_capabillity_buffer_t) / 2; i++) {
        buf[i] = inw(io_base + ATA_REG_DATA);
    }

    read10_capabillity_buffer_t *buf32 = (read10_capabillity_buffer_t *)buffer;
    buf32->sector_size = be32toh(buf32->sector_size);
    buf32->last_lba = be32toh(buf32->last_lba);

    return 0;
}