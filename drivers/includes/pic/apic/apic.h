#ifndef APIC_H
#define APIC_H

#include <stdint.h>
#include "includes/main/cpu/io.h"
#include "includes/main/cpu/isr.h"

#define APIC_EOI                    0xB0
#define APIC_LID                    0xD0
#define APIC_ISR_BASE               0x100
#define APIC_IRR_BASE               0x200
#define APIC_TPR                    0x080
#define APIC_LVT_TIMER              0x320
#define APIC_LVT_LINT0              0x350
#define APIC_LVT_LINT1              0x360

#define APIC_BASE                   0xFEE00000
#define APIC_IO_BASE                0xFEC00000
#define APIC_REGISTER_OFFSET        0x10
#define APIC_ID                     0x20
#define CR4_APIC_BIT                0x200
#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define APIC_DELIVERY_MODE_FIXED    0x00000000
#define APIC_DELIVERY_MODE_LOWEST   0x00000100
#define APIC_DELIVERY_MODE_SMI      0x00000200
#define APIC_DELIVERY_MODE_NMI      0x00000400
#define APIC_DELIVERY_MODE_INIT     0x00000500
#define APIC_DELIVERY_MODE_STARTUP  0x00000600
#define APIC_DELIVERY_MODE_EXTINT   0x00000700
#define APIC_LVT_INT_MASKED         0x00000100
#define APIC_LVT_TRIGGER_LEVEL      0x00000080

#define APIC_DELIVERY_MODE_MASK      0x00000700

#define APIC_SVR            0xF0
#define APIC_DEST_FORMAT    0xE0

#define IOAPIC_REG_SELECT     0x00
#define IOAPIC_REG_WINDOW     0x10

extern volatile uint32_t* apic_base;
extern volatile uint32_t* apic_io_base;
void cpuSetMSR(uint32_t msr, uint32_t low, uint32_t high);
void cpuGetMSR(uint32_t msr, uint32_t* low, uint32_t* high);

static inline uint64_t ReadCR4() {
    uint64_t cr4;
    asm("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

static inline void WriteCR4(uint64_t cr4) {
    asm("mov %0, %%cr4" :: "r"(cr4));
}

static inline void APIC_Write(uint32_t reg, uint32_t value) {
    apic_base[reg / sizeof(uint32_t)] = value;
}

static inline uint32_t APIC_Read(uint32_t reg) {
    return apic_base[reg / sizeof(uint32_t)];
}

static inline uint32_t APIC_ReadIO(uint32_t reg) {

    apic_io_base[IOAPIC_REG_SELECT / sizeof(uint32_t)] = reg;

    return apic_io_base[IOAPIC_REG_WINDOW / sizeof(uint32_t)];
}

static inline void APIC_WriteIO(uint32_t reg, uint32_t value) {

    apic_io_base[IOAPIC_REG_SELECT / sizeof(uint32_t)] = reg;

    apic_io_base[IOAPIC_REG_WINDOW / sizeof(uint32_t)] = value;
}

void cpu_set_apic_base(uintptr_t apic);
uintptr_t cpu_get_apic_base();

uint32_t cpuReadIoApic(void *ioapicaddr, uint32_t reg);
void cpuWriteIoApic(void *ioapicaddr, uint32_t reg, uint32_t value);

void ConfigureIMCR();
void APIC_Initialize();

static inline void LAPIC_SendEOI() {
    APIC_Write(APIC_EOI, 0);
}

#endif