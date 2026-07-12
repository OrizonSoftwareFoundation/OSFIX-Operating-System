/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */


#include "includes/main/cpu/isr.h"
#include "includes/main/cpu/idt.h"
#include "includes/main/cpu/gdt.h"
#include "includes/main/cpu/io.h"
#include <serial.h>
#include <stddef.h>
#include <log.h>
#include "kernel/main/terminal/src/flanterm.h"
#include <kprintf.h>

extern struct flanterm_context *global_flanterm;

ISRHandler_t g_ISRHandlers[256];

extern void ISR_InitializeGates();

void ISR_Initialize() {
    ISR_InitializeGates();
    ISR_RegisterHandler(14, page_fault_handler);
    for (int i = 0; i < 256; i++) {
        if (i != 14)
            IDT_EnableGate(i);
    }
    LOG_OK("ISR initialized successfully\n");
    SERIAL(Info, ISR_Initialize, "ISR initialized successfully\n");
}

static void dump_regs(Registers_t *regs) {
    kprintf("------------------EXTENDED REGISTER REPORT-----------------\n");

    uint64_t cr0, cr2, cr3, cr4;
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;

    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdt, idt;

    __asm__ volatile ("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile ("mov %%cr2,%0":"=r"(cr2));
    __asm__ volatile ("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile ("mov %%cr4,%0":"=r"(cr4));

    __asm__ volatile ("mov %%dr0,%0":"=r"(dr0));
    __asm__ volatile ("mov %%dr1,%0":"=r"(dr1));
    __asm__ volatile ("mov %%dr2,%0":"=r"(dr2));
    __asm__ volatile ("mov %%dr3,%0":"=r"(dr3));
    __asm__ volatile ("mov %%dr6,%0":"=r"(dr6));
    __asm__ volatile ("mov %%dr7,%0":"=r"(dr7));

    __asm__ volatile ("sgdt %0":"=m"(gdt));
    __asm__ volatile ("sidt %0":"=m"(idt));

    kprintf("  rax=%llx rbx=%llx rcx=%llx rdx=%llx\n"
           "  rsi=%llx rdi=%llx rbp=%llx rsp=%llx\n"
           "  r8=%llx r9=%llx r10=%llx r11=%llx\n"
           "  r12=%llx r13=%llx r14=%llx r15=%llx\n"
           "  rip=%llx rflags=%llx\n",
        regs->rax, regs->rbx, regs->rcx, regs->rdx,
        regs->rsi, regs->rdi, regs->rbp, regs->rsp,
        regs->r8, regs->r9, regs->r10, regs->r11,
        regs->r12, regs->r13, regs->r14, regs->r15,
        regs->rip, regs->rflags);

    kprintf("  cs=%llx ss=%llx\n", regs->cs, regs->ss);

    kprintf("  cr0=%llx cr2=%llx cr3=%llx cr4=%llx\n", cr0, cr2, cr3, cr4);
    kprintf("  dr0=%llx dr1=%llx dr2=%llx dr3=%llx dr6=%llx dr7=%llx\n",
        dr0, dr1, dr2, dr3, dr6, dr7);

    kprintf("  GDTR base=%llx limit=%x\n"
           "  IDTR base=%llx limit=%x\n",
        gdt.base, (uint32_t)gdt.limit,
        idt.base, (uint32_t)idt.limit);

    kprintf("  interrupt=%llu errorcode=%llx\n",
        regs->interrupt, regs->error);
    
    serial_printf("------------------EXTENDED REGISTER REPORT-----------------\n");
    serial_printf("  rax=%llx rbx=%llx rcx=%llx rdx=%llx\n"
           "  rsi=%llx rdi=%llx rbp=%llx rsp=%llx\n"
           "  r8=%llx r9=%llx r10=%llx r11=%llx\n"
           "  r12=%llx r13=%llx r14=%llx r15=%llx\n"
           "  rip=%llx rflags=%llx\n",
        regs->rax, regs->rbx, regs->rcx, regs->rdx,
        regs->rsi, regs->rdi, regs->rbp, regs->rsp,
        regs->r8, regs->r9, regs->r10, regs->r11,
        regs->r12, regs->r13, regs->r14, regs->r15,
        regs->rip, regs->rflags);
    serial_printf("  cs=%llx ss=%llx\n", regs->cs, regs->ss);
    serial_printf("  cr0=%llx cr2=%llx cr3=%llx cr4=%llx\n", cr0, cr2, cr3, cr4);
    serial_printf("  dr0=%llx dr1=%llx dr2=%llx dr3=%llx dr6=%llx dr7=%llx\n",
        dr0, dr1, dr2, dr3, dr6, dr7);
    serial_printf("  GDTR base=%llx limit=%x\n"
           "  IDTR base=%llx limit=%x\n",
        gdt.base, (uint32_t)gdt.limit,
        idt.base, (uint32_t)idt.limit); 
    serial_printf("  interrupt=%llu errorcode=%llx\n",
        regs->interrupt, regs->error);
        
}

static void print_verbose_isr_info(Registers_t *regs) {
    const char *mode = (regs->cs & 3) == 3 ? "User Mode (Ring 3)" : "Kernel Mode (Ring 0)";

    if (regs->interrupt == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

        LOG_BOTH("\n--- EXCEPTION REPORT ---\n");
        LOG_BOTH("[Context]\n");

        LOG_BOTH("  Interrupt: %llu\n", regs->interrupt);

        LOG_BOTH("\n[Execution]\n");
        LOG_BOTH("  RIP: %llx\n", regs->rip);
        LOG_BOTH("  Mode: %s\n", mode);
        LOG_BOTH("  CS: %llx\n", regs->cs);

        LOG_BOTH("\n[Memory Fault]\n");
        LOG_BOTH("  CR2: %llx\n", cr2);
        LOG_BOTH("  Error: %llx\n", regs->error);

    } else if (regs->interrupt == 13) {
        LOG_BOTH("\n[GP Fault]\n");
        LOG_BOTH("  Error: %llx\n", regs->error);
    }

    LOG_BOTH("\n[Stack]\n");

    uint64_t *frame = (uint64_t *)regs->rbp;
    int depth = 0;

    while (frame && depth < 15) {
        uint64_t ret = frame[1];
        LOG_BOTH("  #%d: [<%llx>]\n", depth, ret);

        if (!frame[0] || frame[0] <= (uint64_t)frame)
            break;

        frame = (uint64_t *)frame[0];
        depth++;
    }
}

void ISR_Handler(Registers_t *regs) {
    if (g_ISRHandlers[regs->interrupt]) {
        g_ISRHandlers[regs->interrupt](regs);
        return;
    }

    printcol(COLOR_RED, "KERNEL PANIC!\n");
    serial_write("KERNEL PANIC!\n", 15);

    kprintf("Unhandled interrupt %llu!\n", regs->interrupt);
    serial_printf("Unhandled interrupt %llu!\n", regs->interrupt);

    print_verbose_isr_info(regs);
    dump_regs(regs);

    halt();
}

void page_fault_handler(Registers_t *regs) {
    printcol(COLOR_RED, "KERNEL PANIC!\n");
    serial_write("KERNEL PANIC!\n", 15);

    printcol(COLOR_LIGHTRED, "PAGE FAULT!\n");
    serial_write("PAGE FAULT!\n", 12);

    print_verbose_isr_info(regs);
    dump_regs(regs);

    halt();
}

void ISR_RegisterHandler(int interrupt, ISRHandler_t handler) {
    g_ISRHandlers[interrupt] = handler;
    IDT_EnableGate(interrupt);
}

void panic(Registers_t *regs) {
    dump_regs(regs);
    halt();
}