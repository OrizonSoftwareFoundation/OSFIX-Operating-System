/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#include "includes/main/cpu/gdt.h"
#include "includes/main/cpu/idt.h"
#include <stdint.h>
#include "boot/limine.h"
#include "includes/main/cpu/io.h"
#include <log.h>
#include <string.h>

struct tss_entry tss;

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

extern void syscall_handler_asm(void);

 
typedef struct {
    uint16_t LimitLow;
    uint16_t BaseLow;
    uint8_t BaseMiddle;
    uint8_t Access;
    uint8_t FlagsLimitHi;
    uint8_t BaseHigh;
} __attribute__((packed)) GDTEntry_t;

typedef struct {
    uint16_t Limit;
    GDTEntry_t *Ptr;
} __attribute__((packed)) GDTDescriptor_t;

 
typedef enum{
    GDT_ACCESS_CODE_READABLE                = 0x02,
    GDT_ACCESS_DATA_WRITEABLE               = 0x02,
    GDT_ACCESS_CODE_CONFORMING              = 0x04,
    GDT_ACCESS_DATA_DIRECTION_NORMAL        = 0x00,
    GDT_ACCESS_DATA_DIRECTION_DOWN          = 0x04,

    GDT_ACCESS_DATA_SEGMENT                 = 0x10,
    GDT_ACCESS_CODE_SEGMENT                 = 0x18,

    GDT_ACCESS_DESCRIPTOR_TSS               = 0x00,

    GDT_ACCESS_RING0                        = 0x00,  
    GDT_ACCESS_RING1                        = 0x20, 
    GDT_ACCESS_RING2                        = 0x40, 
    GDT_ACCESS_RING3                        = 0x60, 

    GDT_ACCESS_PRESENT                      = 0x80,

} GDT_ACCESS_t;

 
typedef enum {
    GDT_FLAG_64BIT                          = 0x20,
    GDT_FLAG_32BIT                          = 0x40,  
    GDT_FLAG_16BIT                          = 0x00,  

    GDT_FLAG_GRANULARITY_1B                 = 0x00,  
    GDT_FLAG_GRANULARITY_4K                 = 0x80,  
} GDT_FLAGS_t;

#ifndef GDT_LIMIT_LOW
#define GDT_LIMIT_LOW(limit)                (((limit) & 0xFFFF))
#endif

#ifndef GDT_BASE_LOW
#define GDT_BASE_LOW(base)                  (((base) & 0xFFFF))
#endif

#ifndef GDT_BASE_MIDDLE
#define GDT_BASE_MIDDLE(base)               ((((base) >> 16) & 0xFF))
#endif

#ifndef GDT_FLAGS_LIMIT_HI
#define GDT_FLAGS_LIMIT_HI(limit, flags)    ((((limit) >> 16) & 0xF) | (((flags) & 0xF0)))
#endif

#ifndef GDT_BASE_HIGH
#define GDT_BASE_HIGH(base)                 ((((base) >> 24) & 0xFF))
#endif

#define GDT_ENTRY(base, limit, access, flags) {                     \
    GDT_LIMIT_LOW(limit),                                           \
    GDT_BASE_LOW(base),                                             \
    GDT_BASE_MIDDLE(base),                                          \
    access,                                                         \
    GDT_FLAGS_LIMIT_HI(limit, flags),                               \
    GDT_BASE_HIGH(base)                                             \
}

GDTEntry_t g_GDT[] = {
    GDT_ENTRY(0, 0, 0, 0),

    GDT_ENTRY(
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
        GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE,
        GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K
    ),

    GDT_ENTRY(
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 |
        GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE,
        GDT_FLAG_GRANULARITY_4K
    ),

    GDT_ENTRY(
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 |
        GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE,
        GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K
    ),

    GDT_ENTRY(
        0,
        0xFFFFF,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 |
        GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE,
        GDT_FLAG_GRANULARITY_4K
    ),

    GDT_ENTRY(
        0,
        sizeof(tss) - 1,
        0x89,
        0x00
    ),

    GDT_ENTRY(0, 0, 0, 0),
};

GDTDescriptor_t g_GDTDescriptor = { sizeof(g_GDT) - 1, g_GDT};

void GDT_Load(GDTDescriptor_t *descriptor, uint16_t cs, uint16_t ds)
{
    __asm__ __volatile__ (
        "lgdt (%0)\n"
        "movw %1, %%ds\n"
        "movw %1, %%es\n"
        "movw %1, %%fs\n"
        "movw %1, %%gs\n"
        "movw %1, %%ss\n"
        "pushq %2\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :: "r"(descriptor), "r"(ds), "r"((uint64_t)cs)
        : "rax"
    );
}

uint8_t kernel_stack[16384] __attribute__((aligned(16)));

 
void TSS_Initialize() {
    memset(&tss, 0, sizeof(tss));

    tss.rsp0 = (uint64_t)kernel_stack + sizeof(kernel_stack);

    tss.iopb_offset = sizeof(tss);

    uint64_t tss_base = (uint64_t)&tss;
    g_GDT[5].BaseLow    = tss_base & 0xFFFF;
    g_GDT[5].BaseMiddle = (tss_base >> 16) & 0xFF;
    g_GDT[5].BaseHigh   = (tss_base >> 24) & 0xFF;
    uint32_t *tss_upper = (uint32_t *)&g_GDT[6];
    *tss_upper = (tss_base >> 32) & 0xFFFFFFFF;
}

 
void GDT_Initialize() {
    TSS_Initialize();

    GDT_Load(&g_GDTDescriptor, GDT_CODE_SEGMENT, GDT_DATA_SEGMENT);

    uint16_t cs, ds;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));

    __asm__ volatile("ltr %0" :: "r"((uint16_t)0x28));

    if (cs == GDT_CODE_SEGMENT && ds == GDT_DATA_SEGMENT) {
        LOG_OK("GDT initialized successfully\n");
        SERIAL(Info, GDT_Initialize, "GDT initialized successfully\n");
    } 
}
