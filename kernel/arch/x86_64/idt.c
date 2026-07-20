#include "includes/gdt.h"
#include "includes/idt.h"
#include <stdint.h>
#include "includes/io.h"
#include <kprintf.h>

#ifndef FLAG_SET

#define FLAG_SET(var, mask)   ((var) |= (mask))
#endif

#ifndef FLAG_UNSET

#define FLAG_UNSET(var, mask) ((var) &= ~(mask))
#endif

typedef struct {
    uint16_t BaseLow;         
    uint16_t SegmentSelector; 
    uint8_t  IST;             
    uint8_t  Flags;          
    uint16_t BaseMiddle;     
    uint32_t BaseHigh;       
    uint32_t Reserved;        
} __attribute__((packed)) IDTEntry_t;

typedef struct {
    uint16_t Limit; 
    uint64_t Ptr;   
} __attribute__((packed)) IDTDescriptor_t;

IDTEntry_t g_IDT[256];

IDTDescriptor_t g_IDTDescriptor = { sizeof(g_IDT) - 1, (uint64_t)g_IDT };

void IDT_Load(IDTDescriptor_t *idtDescriptor) {
    __asm__ __volatile__ ("lidt (%0)" :: "r"(idtDescriptor));
}

void IDT_SetGate(int interrupt, void *base, uint16_t segmentDescriptor, uint8_t flags)
{
    uint64_t addr = (uint64_t)base;

    g_IDT[interrupt].BaseLow         = addr & 0xFFFF;
    g_IDT[interrupt].BaseMiddle      = (addr >> 16) & 0xFFFF;
    g_IDT[interrupt].BaseHigh        = (addr >> 32) & 0xFFFFFFFF;

    g_IDT[interrupt].SegmentSelector = segmentDescriptor;
    g_IDT[interrupt].IST             = 0;
    g_IDT[interrupt].Flags           = flags;
    g_IDT[interrupt].Reserved        = 0;
}

void IDT_EnableGate(int interrupt) {
    FLAG_SET(g_IDT[interrupt].Flags, IDT_FLAG_PRESENT);
}

void IDT_DisableGate(int interrupt) {
    FLAG_UNSET(g_IDT[interrupt].Flags, IDT_FLAG_PRESENT);
}

void IDT_Initialize() {
    IDT_Load(&g_IDTDescriptor);

    IDTDescriptor_t current;
    __asm__ volatile("sidt %0" : "=m"(current));

    if (current.Ptr == (uint64_t)g_IDTDescriptor.Ptr &&
        current.Limit == g_IDTDescriptor.Limit) {

        log(Ok, "IDT initialized successfully\n");
    }
}
