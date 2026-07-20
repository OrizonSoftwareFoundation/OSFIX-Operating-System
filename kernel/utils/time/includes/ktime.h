/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <io.h>
#include <math.h>
#include <pit.h>
#include <tsc.h>

typedef enum{
    PIT_DELAY    = 0, 

    PIC_DELAY    = 1, 

    APIC_DELAY   = 2, 

    IOAPIC_DELAY = 3, 

    TSC_DELAY    = 4, 

    HPET_DELAY   = 5, 

} t_delay_mode;

int8_t set_CPU_clock_speed(void);

uint32_t get_time_us(void);

uint32_t get_time_ms(void);

double get_time_ms_fp(void);

int8_t udelay(uint32_t micros);

int8_t tsc_delay_us(uint32_t micros);

#endif
