/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#ifndef TSC_H
#define TSC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "kernel/includes/main/cpu/io.h"
#include <math.h>

extern uint64_t CPU_clock_speed;

extern uint64_t boot_tsc;  

uint64_t measure_tsc_over_pit(uint16_t pit_reload);

uint64_t get_cpu_clock_speed_khz(void);

int8_t tsc_delay_us(uint32_t micros);

static inline bool cpu_has_tsc(void) {
    uint32_t eax, ebx, ecx, edx;
    eax = 1;
    asm("cpuid"
        : "=d"(edx), "=a"(eax), "=b"(ebx), "=c"(ecx)
        : "a"(eax)
        : );
    return (edx & (1 << 4)) != 0;
}

static inline uint64_t read_tsc_serialized(void) {
    uint32_t lo, hi;
    asm(
        "cpuid\n"
        "rdtsc\n"
        : "=a"(lo), "=d"(hi)
        : "a"(0)
        : "ebx", "ecx"
    );
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t read_tsc_fast(void) {
    uint32_t lo, hi;
    asm("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void capture_boot_tsc(void) {
    if (cpu_has_tsc()) {
        boot_tsc = read_tsc_fast();
    }
}

static inline int8_t set_CPU_clock_speed_partial(void) {
    if (CPU_clock_speed == 0)
        CPU_clock_speed = get_cpu_clock_speed_khz() * 1000;
    return CPU_clock_speed == 0 ? -1 : 0; 
}

static inline int8_t set_CPU_clock_speed(void) {
    if (CPU_clock_speed != 0)
        return 0; 
    uint64_t accumulate = 0;
    for (int i=0; i<10; i++) {
        set_CPU_clock_speed_partial();
        if (CPU_clock_speed == 0) 
            return -1;
        accumulate += CPU_clock_speed/1000;
    }
    CPU_clock_speed = accumulate * 100;
    return 0; 
}

#endif
