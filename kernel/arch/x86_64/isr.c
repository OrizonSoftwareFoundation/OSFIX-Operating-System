#include "includes/isr.h"
#include "includes/idt.h"
#include "includes/gdt.h"
#include "includes/io.h"
#include <serial.h>
#include <stddef.h>
#include <kprintf.h>
#include <flanterm.h>
#include <ksyms.h>

extern struct flanterm_context *global_flanterm;

ISRHandler_t g_ISRHandlers[256];
extern void ISR_InitializeGates();

static volatile int in_panic = 0; //stops it from panicking over and over again

//lower bound for "this looks like a real return address" during the rbp-chain walk;
//using the canonical higher-half start instead of a kernel-specific constant means this doesnt shit its pants if
//KERNEL_VMA_OFFSET in linker.ld ever changes (which if you arent demented or just plain stupid it wont change)
#define KADDR_MIN 0xffff800000000000ULL

void ISR_Initialize() {
    ISR_InitializeGates();
    ISR_RegisterHandler(14, page_fault_handler);

    for (int i = 0; i < 256; i++) {
        if (i != 14)
            IDT_EnableGate(i);
    }
    log(Ok, "ISR initialized successfully\n");
}

static const char *exception_name(uint64_t n)
{
    static const char *names[] = {
        "Divide-by-zero",
        "Debug",
        "Non-maskable interrupt",
        "Breakpoint",
        "Overflow",
        "Bound range exceeded",
        "Invalid opcode",
        "Device not available",
        "Double fault",
        "Coprocessor segment overrun",
        "Invalid TSS",
        "Segment not present",
        "Stack-segment fault",
        "General protection fault",
        "Page fault",
        "Reserved",
        "x87 floating-point exception",  //i didnt know x87 existed until i looked it up btw
        "Alignment check",
        "Machine check",
        "SIMD floating-point exception",
        "Virtualization exception",
        "Control protection exception"
    };
    if (n < sizeof(names)/sizeof(names[0]))
        return names[n];
    return "Unknown";
}

static void print_error_code(uint64_t vector, uint64_t err)
{
    if (vector == 14) { //page fault
        kprintf("Error code: %llx\n", err);
        kprintf("  [%c] Present\n",          (err & 1)  ? 'x' : ' ');
        kprintf("  [%c] Write\n",            (err & 2)  ? 'x' : ' ');
        kprintf("  [%c] User\n",             (err & 4)  ? 'x' : ' ');
        kprintf("  [%c] Reserved bit\n",     (err & 8)  ? 'x' : ' ');
        kprintf("  [%c] Instruction fetch\n",(err & 16) ? 'x' : ' ');
        kprintf("  [%c] Protection key\n",   (err & 32) ? 'x' : ' ');
        kprintf("  [%c] Shadow stack\n",     (err & 64) ? 'x' : ' ');
    } else if (vector == 13) { //GPF
        kprintf("Error code: %llx  ", err);
        if (err == 0)
            kprintf("(no segment selector)\n");
        else
            kprintf("(selector index related)\n");
    } else if (err) {
        kprintf("Error code: %llx\n", err);
    }
}

static void dump_registers(Registers_t *regs)
{
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2,%0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4,%0" : "=r"(cr4));

    kprintf("\nRegisters:\n");
    kprintf("RAX: %016llx  RBX: %016llx  RCX: %016llx  RDX: %016llx\n",
            regs->rax, regs->rbx, regs->rcx, regs->rdx);
    kprintf("RSI: %016llx  RDI: %016llx  RBP: %016llx  RSP: %016llx\n",
            regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    kprintf("R8:  %016llx  R9:  %016llx  R10: %016llx  R11: %016llx\n",
            regs->r8,  regs->r9,  regs->r10, regs->r11);
    kprintf("R12: %016llx  R13: %016llx  R14: %016llx  R15: %016llx\n",
            regs->r12, regs->r13, regs->r14, regs->r15);
    kprintf("RIP: %016llx  RFLAGS: %016llx\n", regs->rip, regs->rflags);
    kprintf("CS: %04llx  SS: %04llx\n", regs->cs, regs->ss);
    kprintf("CR0: %016llx  CR2: %016llx  CR3: %016llx  CR4: %016llx\n",
            cr0, cr2, cr3, cr4);
}

//prints function and address, falls back to address-only if it cant find the function (for whatever reason)
static void print_addr(uint64_t addr)
{
    const KSym_t *sym = ksym_find(addr);
    if (sym) {
        kprintf("  [<%016llx>] %s+%llx\n", addr, sym->name, addr - sym->addr);
    } else {
        kprintf("  [<%016llx>] ?\n", addr);
    }
}

static void print_call_trace(Registers_t *regs)
{
    kprintf("\nCall Trace:\n");
    kprintf(" <TASK>\n");

    print_addr(regs->rip);

    uint64_t *frame = (uint64_t *)regs->rbp;
    int depth = 0;

    while (frame && depth < 20) {
        uint64_t ret = frame[1];

        if (ret < KADDR_MIN)
            break;
        if (frame[0] <= (uint64_t)frame)
            break;

        print_addr(ret);

        frame = (uint64_t *)frame[0];
        depth++;
    }
    kprintf(" </TASK>\n");
}


static void do_panic(Registers_t *regs, const char *extra_msg)
{
    if (in_panic) {
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    in_panic = 1;

    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    kprintf("\n");
    kprintf("%CKERNEL PANIC%C\n", COLOR_RED, COLOR_DIM);

    if (regs->interrupt == 14) {
        kprintf("\nBUG: unable to handle kernel paging request at %016llx\n", cr2);
    } else {
        kprintf("\nBUG: %s\n", exception_name(regs->interrupt));
    }

    kprintf("Oops: %04llx [#1]\n", regs->error);
    kprintf("CPU: 0, PID: 0, Comm: swapper\n");

    const KSym_t *rip_sym = ksym_find(regs->rip);
    if (rip_sym) {
        kprintf("RIP: %04llx:[<%016llx>] %s+%llx\n",
                regs->cs, regs->rip, rip_sym->name, regs->rip - rip_sym->addr);
    } else {
        kprintf("RIP: %04llx:[<%016llx>]\n", regs->cs, regs->rip);
    }
    kprintf("Code: (no disassembly yet)\n");

    print_error_code(regs->interrupt, regs->error);
    dump_registers(regs);
    print_call_trace(regs);

    if (extra_msg)
        kprintf("\n%s\n", extra_msg);

    kprintf("\n[ end Kernel panic - not syncing: Fatal exception ]\n");
    halt();
}

void ISR_Handler(Registers_t *regs)
{
    if (g_ISRHandlers[regs->interrupt]) {
        g_ISRHandlers[regs->interrupt](regs);
        return;
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "Unhandled exception %llu (%s)",
              regs->interrupt, exception_name(regs->interrupt));

    do_panic(regs, buf);
}
void page_fault_handler(Registers_t *regs)
{
    do_panic(regs, "Page fault triggered, halting kernel...");
}

void ISR_RegisterHandler(int interrupt, ISRHandler_t handler)
{
    g_ISRHandlers[interrupt] = handler;
    IDT_EnableGate(interrupt);
}

void kpanic(Registers_t *regs)
{
    do_panic(regs, "Explicit kpanic() called");
    //AAAAHHHHHHHHHHHHHHHHHHHHHHHHHHHH
}