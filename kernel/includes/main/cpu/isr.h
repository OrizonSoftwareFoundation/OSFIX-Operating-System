/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */


#ifndef ISR_H
#define ISR_H

#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8; 

    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;     

    uint64_t interrupt, error;                     

    uint64_t rip, cs, rflags, rsp, ss;             

} __attribute__((packed)) Registers_t;

static const char *const g_Exceptions[] = {
    "Divide by zero error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "",
    "x86 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception ",
    "",
    "",
    "",
    "",
    "",
    "",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    ""
};

typedef void (*ISRHandler_t)(Registers_t *regs);

void ISR_Initialize();

void ISR_RegisterHandler(int interrupt, ISRHandler_t handler);

void page_fault_handler(Registers_t *regs);

void panic(Registers_t *regs);

#endif
