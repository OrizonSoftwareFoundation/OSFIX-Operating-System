/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kernel/includes/main/cpu/io.h"
#include <math.h>

#define PIT_CHANNEL0_PORT 0x40

#define PIT_CMD_PORT      0x43

#define PIT_FREQ          1193182u

uint16_t pit_read_counter();

void pit_start_one_shot(uint16_t reload);

void pit_delay_us_limited(uint32_t micros);

void pit_delay_us(uint32_t micros);

uint8_t pit_get_is_legit();

#endif
