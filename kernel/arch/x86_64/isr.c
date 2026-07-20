#include "includes/isr.h"
#include "includes/idt.h"
#include "includes/gdt.h"
#include "includes/io.h"
#include <serial.h>
#include <stddef.h>
#include <kprintf.h>
#include <flanterm.h>
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
    log(Ok, "ISR initialized successfully\n");
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
    
        
}

static void print_verbose_isr_info(Registers_t *regs) {
    const char *mode = (regs->cs & 3) == 3 ? "User Mode (Ring 3)" : "Kernel Mode (Ring 0)";

    
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

        kprintf("\n--- EXCEPTION REPORT ---\n");
        kprintf("[Context]\n");

        kprintf("  Interrupt: %llu\n", regs->interrupt);

        kprintf("\n[Execution]\n");
        kprintf("  RIP: %llx\n", regs->rip);
        kprintf("  Mode: %s\n", mode);
        kprintf("  CS: %llx\n", regs->cs);

        kprintf("\n[Memory Fault]\n");
        kprintf("  CR2: %llx\n", cr2);
        kprintf("  Error: %llx\n", regs->error);

     if (regs->interrupt == 13) {
        kprintf("\n[GP Fault]\n");
        kprintf("  Error: %llx\n", regs->error);
    }

    kprintf("\n[Stack]\n");

    uint64_t *frame = (uint64_t *)regs->rbp;
    int depth = 0;

    while (frame && depth < 15) {
        uint64_t ret = frame[1];
        kprintf("  #%d: [<%llx>]\n", depth, ret);

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

    if(regs->interrupt != 0){
    kprintf("\n%CKERNEL PANIC!%C\n", COLOR_RED, COLOR_DIM);
    kprintf("Unhandled interrupt %llu!\n", regs->interrupt);
    }
    else{
    kprintf("\n%CKERNEL PANIC!%C\n", COLOR_RED, COLOR_DIM);
    kprintf("Divide by Zero Error!\n", regs->interrupt);  
    }

    print_verbose_isr_info(regs);
    dump_regs(regs);
    halt();
}

void page_fault_handler(Registers_t *regs) {
    

    kprintf("\n%CKERNEL PANIC!%C\n", COLOR_RED, COLOR_DIM);
    printcol(COLOR_LIGHTRED, "PAGE FAULT!\n");

    dump_regs(regs);
    print_verbose_isr_info(regs);

    halt();
}

void ISR_RegisterHandler(int interrupt, ISRHandler_t handler) {
    g_ISRHandlers[interrupt] = handler;
    IDT_EnableGate(interrupt);
}

void kpanic(Registers_t *regs) {
    dump_regs(regs);
    halt();
}