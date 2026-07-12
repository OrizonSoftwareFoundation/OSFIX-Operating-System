/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <util.h>
#include "kernel/includes/main/cpu/io.h"
#include <math.h>
#include <time/pit.h>
#include <time/tsc.h>

uint16_t pit_read_counter() {
    uint8_t lo, hi;
    outb(PIT_CMD_PORT, 0x00);
    lo = inb(PIT_CHANNEL0_PORT);
    hi = inb(PIT_CHANNEL0_PORT);
    return ((uint16_t)hi << 8) | lo;
}

void pit_start_one_shot(uint16_t reload) {
    outb(PIT_CMD_PORT, 0x34);
    outb(PIT_CHANNEL0_PORT, reload & 0xFF);
    outb(PIT_CHANNEL0_PORT, reload >> 8);
}

void pit_delay_us(uint32_t micros) {
    if (micros > 25000) {

        for (uint32_t i = 0; i < micros / 25000; i++) {
            pit_delay_us_limited(25000);
        }
        pit_delay_us_limited(micros % 25000);
        return;
    }
    pit_delay_us_limited(micros);
}

void pit_delay_us_limited(uint32_t micros) {
    if (micros == 0) return;

    uint16_t pit_reload = (uint16_t)((PIT_FREQ / 1000000) * micros);
    if (pit_reload == 0) pit_reload = 1;

    pit_start_one_shot(pit_reload * 2);

    while (pit_read_counter() > pit_reload);

    return;
}

uint8_t pit_get_is_legit() {
    pit_start_one_shot(8);
    uint8_t old_pit_counter = pit_read_counter();
    if (!cpu_has_tsc()) {
        return 1;
    }

    uint64_t base_tsc = read_tsc_serialized();
    while (pit_read_counter() == old_pit_counter && read_tsc_serialized()-base_tsc > 24000);

    return (uint8_t)(read_tsc_serialized()-base_tsc > 24000);
}
