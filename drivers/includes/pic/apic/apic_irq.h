#ifndef APIC_IRQ_H
#define APIC_IRQ_H
#include "includes/main/cpu/isr.h"
#include "includes/main/cpu/io.h"
typedef void (*IRQHandler_t)(Registers_t *regs);
extern IRQHandler_t g_APICIRQHandler_ts[64];

void APIC_IRQ_Handler(Registers_t* regs);       
void APIC_IRQ_Initialize();                  
void APIC_IRQ_RegisterHandler(uint32_t irq, IRQHandler_t handler); 
void APIC_EnableIRQ(uint32_t irq);               
void APIC_DisableIRQ(uint32_t irq);           
void APIC_EnableIOIRQ(uint32_t irq, uint32_t interrupt_destination);
void APIC_DisableIOIRQ(uint32_t irq);

#endif 
