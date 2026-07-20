/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <io.h>
#include <util.h>
#include <math.h>
#include <pit.h>
#include <tsc.h>
#include <ktime.h>
t_delay_mode microdelay_mode = TSC_DELAY;

uint32_t get_time_ms(void) {
    int8_t error = set_CPU_clock_speed();

    if (error || boot_tsc == 0)
        return 0; 
    
    uint64_t current_tsc = read_tsc_fast();
    uint64_t elapsed_tsc = current_tsc - boot_tsc;
    
    uint64_t ms = (elapsed_tsc * 1000) / CPU_clock_speed;
    return (uint32_t)ms;
}

double get_time_ms_fp(void) {
    int8_t error = set_CPU_clock_speed();
    if (error || boot_tsc == 0)
        return 0.0;

    uint64_t current_tsc = read_tsc_fast();
    uint64_t elapsed_tsc = current_tsc - boot_tsc;
    
    double seconds = (double)elapsed_tsc / CPU_clock_speed;
    return seconds * 1000.0;
}

uint32_t get_time_us(void) {
    int8_t error = set_CPU_clock_speed();

    if (error || boot_tsc == 0)
        return 0;
    
    uint64_t current_tsc = read_tsc_fast();
    uint64_t elapsed_tsc = current_tsc - boot_tsc;
    
    uint64_t cycles_per_us = CPU_clock_speed / 1000000;
    return (uint32_t)(elapsed_tsc / cycles_per_us);
}

int8_t udelay(uint32_t micros) {
    if (microdelay_mode == PIT_DELAY && !pit_get_is_legit()) {
        microdelay_mode = TSC_DELAY;
    }
    if (microdelay_mode == TSC_DELAY && !cpu_has_tsc()) {
        microdelay_mode = HPET_DELAY;
    }
    switch (microdelay_mode) {
        case PIT_DELAY:
            pit_delay_us(micros); return 0;
        case PIC_DELAY:
            return -1;
        case APIC_DELAY:
            return -1;
        case IOAPIC_DELAY:
            return -1;
        case TSC_DELAY:
            return tsc_delay_us(micros);
        case HPET_DELAY:
            return -1;
        default:
            return -1;
    }
}
