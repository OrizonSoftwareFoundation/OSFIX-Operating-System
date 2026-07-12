#ifndef SCSIUTILS_H
#define SCSIUTILS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../kernel/includes/main/util/util.h"

typedef struct {
    uint32_t last_lba;    

    uint32_t sector_size; 

} read10_capabillity_buffer_t;

static inline void build_read_capacity_atapi_cmd(uint8_t *cmd) {
    memset(cmd, 0, 10);
    cmd[0] = 0x25;
}

static inline void build_read10_atapi_cmd(uint8_t *cmd, uint32_t lba, uint16_t sector_count) {
    memset(cmd, 0, 12);
    cmd[0] = 0x28;
    cmd[2] = (lba >> 24) & 0xFF;
    cmd[3] = (lba >> 16) & 0xFF;
    cmd[4] = (lba >> 8) & 0xFF;
    cmd[5] = lba & 0xFF;
    cmd[7] = (sector_count >> 8) & 0xFF;
    cmd[8] = sector_count & 0xFF;
}

#endif
