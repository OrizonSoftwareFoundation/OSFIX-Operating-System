/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kernel/includes/main/cpu/io.h"
#include <util.h>
#include <math.h>
#include <time/pit.h>
#include <time/tsc.h>
#include <time/ktime.h>

uint64_t CPU_clock_speed = 0;

uint64_t boot_tsc = 0;

uint64_t measure_tsc_over_pit(uint16_t pit_reload) {
    if (pit_reload == 0 || !pit_get_is_legit()) 
        return 0;

    bool unary = pit_reload <= 0x7FFF;
    uint16_t tickrate = unary ? pit_reload*2 : pit_reload;
    uint16_t skiprate = unary ? pit_reload   : 0;
    pit_start_one_shot(tickrate);
    uint64_t tsc_start = read_tsc_serialized();

    while (pit_read_counter() > skiprate);

    uint64_t tsc_end = read_tsc_serialized();
    uint16_t remaining_ticks = pit_read_counter();

    uint16_t elapsed_ticks = tickrate - remaining_ticks;
    uint64_t tsc_estimate = tsc_end - tsc_start;
    if (elapsed_ticks > 0 && elapsed_ticks != pit_reload) {
        tsc_estimate = (tsc_estimate * pit_reload) / elapsed_ticks;
    }

    return tsc_estimate;
}

uint64_t get_cpu_clock_speed_khz(void) {
    if (!cpu_has_tsc()) return 0;
    uint64_t tsc_in_1_ltu = measure_tsc_over_pit(1193*4);
    if (tsc_in_1_ltu == 0) return 0;

    uint64_t CPU_KHz = tsc_in_1_ltu * 1000000000 / 999848528;
    return CPU_KHz / 4;
}

int8_t tsc_delay_us(uint32_t micros) {
    uint32_t last_recorded_time, start_time;
    start_time = get_time_us();
    last_recorded_time = start_time;

    if (start_time == 0)
        return -1;
    
    while (last_recorded_time != 0 && last_recorded_time-start_time < micros) {
        last_recorded_time = get_time_us();
    }
    return last_recorded_time == 0 ? -1 : 0;
}
