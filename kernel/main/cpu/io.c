/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */


#include <stdint.h>
#include "includes/main/cpu/io.h"
#include <kprintf.h>
#include <stdbool.h>
#include <serial.h>

void halt(void) {
    uint64_t rip;

    __asm__ volatile (
        "leaq (%%rip), %0"
        : "=r"(rip)
    );

    kprintf("[ EMERGENCY ] HALTED CPU AT INSTRUCTION: %llx\n", rip);
    serial_write("[ EMERGENCY ] HALTED CPU AT INSTRUCTION: %llx\n", rip);

    __asm__ volatile (
        "cli\n"
        "1:\n"
        "hlt\n"
        "jmp 1b\n"
    );
}

void halt_interrupts_enabled() {
    uint64_t rip;

    __asm__ volatile (
        "leaq (%%rip), %0"
        : "=r"(rip)
    );

    kprintf("[ HALT ] Halted CPU at instruction: 0x%llx\n", rip);
    serial_write("[ HALT ] CPU halted at instruction: 0x%llx\n", rip);

    __asm__ volatile (
        "1:\n"
        "hlt\n"
        "jmp 1b\n"
    );
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

bool are_interrupts_enabled(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    return (flags & (1 << 9)) != 0;
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");

    if (are_interrupts_enabled()) {
        kprintf("Successfully enabled interrupts\n");
        serial_write("Successfully enabled interrupts\n");
    } else {
        kprintf("Failed to enable interrupts\n");
        serial_write("Failed to enable interrupts\n");
        halt();
    }
}

void disable_interrupts(void) {
    __asm__ volatile ("cli");

    if (!are_interrupts_enabled()) {
        kprintf("Successfully disabled interrupts\n");
        serial_write("Successfully disabled interrupts\n");
    } else {
        kprintf("Failed to disable interrupts\n");
        serial_write("Failed to disable interrupts\n");
        halt();
    }
}

void cpuid(uint32_t leaf, uint32_t subleaf,
           uint32_t *eax, uint32_t *ebx,
           uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx),
                       "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}
