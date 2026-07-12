#include "../../includes/pic/apic/apic.h"
#include "../../includes/pic/apic/apic_irq.h"
#include "includes/main/cpu/idt.h"
#include "includes/main/cpu/isr.h"
#include "includes/main/cpu/io.h"
#include <log.h>

volatile uint32_t* apic_base = (volatile uint32_t*)APIC_BASE;
volatile uint32_t* apic_io_base = (volatile uint32_t*)APIC_IO_BASE;
uint32_t local_apic_address;
uint32_t local_ioapic_address;
uint32_t apic_flags;
uint32_t *apic_entries;

void cpu_set_apic_base(uintptr_t apic) {
   uint32_t edx = 0;
   uint32_t eax = (apic & 0xFFFFF000) | IA32_APIC_BASE_MSR_ENABLE;

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   edx = (apic >> 32) & 0x0F;
#endif

   cpuSetMSR(IA32_APIC_BASE_MSR, eax, edx);
}

uintptr_t cpu_get_apic_base(void) {
   uint32_t eax, edx;
   cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);

#ifdef __PHYSICAL_MEMORY_EXTENSION__
   return (eax & 0xFFFFF000) | ((edx & 0x0F) << 32);
#else
   return (eax & 0xFFFFF000);
#endif
}

void ConfigureIMCR(void) {
    uint8_t al = inb(0x22);
    if (al & 0x80) {
        outb(0x22, 0x70);
        outb(0x23, 0x1);
    }
}

void APIC_Initialize(void) {
    uint32_t eax, edx;

    uint64_t cr4 = ReadCR4();
    cr4 |= CR4_APIC_BIT;
    WriteCR4(cr4);
    
    LOG_OK("APIC enabled in CR4\n");
    SERIAL(Info, APIC_Initialize, "APIC enabled in CR4\n");
    

    cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);
    
    if (!(eax & IA32_APIC_BASE_MSR_ENABLE)) {
    
        eax |= IA32_APIC_BASE_MSR_ENABLE;
        cpuSetMSR(IA32_APIC_BASE_MSR, eax, edx);
        LOG_OK("Enabled APIC in MSR\n");
        SERIAL(Info, APIC_Initialize, "Enabled APIC in MSR\n");
    }
    

    cpuGetMSR(IA32_APIC_BASE_MSR, &eax, &edx);
    if (eax & IA32_APIC_BASE_MSR_ENABLE) {
        LOG_OK( "APIC enabled and verified in MSR\n");
        SERIAL(Info, APIC_Initialize, "APIC enabled and verified in MSR\n");
    } 
    else {
        LOG_FATAL("Failed to enable APIC in MSR!\n");
        SERIAL(Fatal, APIC_Initialize, "Failed to enable APIC in MSR!\n");
    }
    

    APIC_Write(APIC_SVR, APIC_Read(APIC_SVR) | 0x100 | 0xFF);
    LOG_OK("APIC initialized successfully\n");
    SERIAL(Info, APIC_Initialize, "APIC initialized successfully\n");

}

static void pic_disable(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);
}

int module_init(void) {
    pic_disable();
    APIC_Initialize();
    return 0;
}