/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */


#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_FLAGS_LIMIT_HI(limit, flags) \
    ((((limit) >> 16) & 0xF) | (((flags) & 0xF0)))

#define GDT_CODE_SEGMENT 0x08

#define GDT_DATA_SEGMENT 0x10

struct __attribute__((packed)) tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;         

    uint64_t rsp1;         

    uint64_t rsp2;         

    uint64_t reserved1;
    uint64_t ist1;         

    uint64_t ist2;         

    uint64_t ist3;         

    uint64_t ist4;         

    uint64_t ist5;         

    uint64_t ist6;         

    uint64_t ist7;         

    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;  

};

extern struct tss_entry tss;

void GDT_Initialize();

#endif
