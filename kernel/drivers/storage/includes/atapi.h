#ifndef DRIVERS_ATAPI_H
#define DRIVERS_ATAPI_H

#include <stdint.h>
#include <stdbool.h>
#include "scsi.h"

#define ATAPI_CMD_PACKET    0xA0

#define ATAPI_CMD_READ10    0x28

#define ATAPI_CMD_IDENTIFY  0xA1

#define ATA_REG_FEATURES    1  

#ifdef __cplusplus
extern "C" {
#endif

static const char *const atapi_errors[] = {
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "NO DATA", "DISK NOT READY", "DISK BUSY",
};

extern uint8_t atapi_status;

extern uint8_t atapi_error;

int atapi_read_sector(uint32_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint32_t sector_size);

int atapi_read_sector_manual(uint32_t lba, uint16_t sector_count, void *buffer, uint8_t drive, uint16_t pmio_base, uint32_t sector_size);

int identify_atapi(uint8_t drive, uint16_t identify_data[256]);

int identify_atapi_manual(uint8_t drive, uint16_t identify_data[256], uint16_t pmio_base);

int identify_atapi_useful(read10_capabillity_buffer_t *buffer, uint8_t drive);

int identify_atapi_useful_manual(read10_capabillity_buffer_t *buffer, uint8_t drive, uint16_t pmio_base);

#ifdef __cplusplus
}
#endif

#endif
