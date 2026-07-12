#include "../../includes/pic/apic/apic.h"
#include "../../includes/pic/apic/apic_irq.h"
#include "includes/main/cpu/idt.h"
#include "includes/main/cpu/isr.h"
#include "includes/main/cpu/io.h"
#include <log.h>
#include <serial.h>
#include <stddef.h>

#define APIC_REMAP_OFFSET        0x20
#define MAX_IRQS                 64

#define APIC_ISR  0x100  
#define APIC_IRR  0x200 

IRQHandler_t g_APICIRQHandler_ts[MAX_IRQS];

void APIC_IRQ_Handler(Registers_t* regs) {
    int irq = regs->interrupt - APIC_REMAP_OFFSET;

    uint32_t apic_isr = APIC_Read(APIC_ISR);  
    uint32_t apic_irr = APIC_Read(APIC_IRR);  

    if (g_APICIRQHandler_ts[irq] != NULL) {

        g_APICIRQHandler_ts[irq](regs);
    } else {
        LOG_WARN("Unhandled APIC IRQ %d  ISR=%x  IRR=%x...\n", irq, apic_isr, apic_irr);
        SERIAL(Warn, APIC_IRQ_Handler, "Unhandled APIC IRQ %d  ISR=%x  IRR=%x...\n", irq, apic_isr, apic_irr);
    }

    LAPIC_SendEOI();
}

void APIC_IRQ_Initialize() {
    APIC_Initialize();

    

    for (int i = 0; i < MAX_IRQS; i++) {
        ISR_RegisterHandler(APIC_REMAP_OFFSET + i, APIC_IRQ_Handler);
    }

    LOG_OK("APIC IRQ initialized successfully\n");
    SERIAL(Info, APIC_IRQ_Initialize, "APIC IRQ initialized successfully\n");
}

void APIC_IRQ_RegisterHandler(uint32_t irq, IRQHandler_t handler) {
    if (irq >= MAX_IRQS) {
        LOG_WARN("Invalid IRQ number %u\n", irq);
        serial_write("Invalid IRQ number\n", 22);
        return;
    }

    g_APICIRQHandler_ts[irq] = handler;
}

void APIC_EnableIRQ(uint32_t irq) {
    uint32_t value = APIC_Read(APIC_LVT_TIMER + irq * APIC_REGISTER_OFFSET);
    value &= ~0x10000;
    APIC_Write(APIC_LVT_TIMER + irq * APIC_REGISTER_OFFSET, value);
}

void APIC_DisableIRQ(uint32_t irq) {
    uint32_t value = APIC_Read(APIC_LVT_TIMER + irq * APIC_REGISTER_OFFSET);
    value |= 0x10000;
    APIC_Write(APIC_LVT_TIMER + irq * APIC_REGISTER_OFFSET, value);
}

  

void APIC_EnableIOIRQ(uint32_t irq, uint32_t interrupt_destination) {
    uint32_t redirection_entry = irq * APIC_REGISTER_OFFSET;

    uint32_t value = (interrupt_destination | APIC_DELIVERY_MODE_FIXED);
    value &= ~0x10000;

    APIC_WriteIO(redirection_entry, value);
}

void APIC_DisableIOIRQ(uint32_t irq) {
    uint32_t redirection_entry = irq * APIC_REGISTER_OFFSET;

    uint32_t value = APIC_ReadIO(redirection_entry);
    value |= 0x10000;  
    APIC_WriteIO(redirection_entry, value);
}