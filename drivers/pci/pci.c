#include <stdint.h>
#include <stdlib.h>
#include "../includes/pci/pci.h"
#include "kernel/includes/main/cpu/io.h"
#include "../includes/storage/sata.h"
#include <time/ktime.h>
#include <log.h>
#include <mm/vmm.h>
#include <kprintf.h>
static uint64_t ehci_mmio_phys = 0;
uint64_t pci_get_ehci_mmio(void) { return ehci_mmio_phys; }

#ifndef PTE_CACHE_DISABLE
#define PTE_CACHE_DISABLE   (1ULL << 4)
#endif

#ifndef PTE_WRITE_THROUGH
#define PTE_WRITE_THROUGH   (1ULL << 3)
#endif

#ifndef PTE_PRESENT
#define PTE_PRESENT         (1ULL << 0)
#endif

uint32_t pid_rn = 0;

static uint8_t  ehci_saved_bus    = 0;
static uint8_t  ehci_saved_device = 0;
static uint8_t  ehci_saved_func   = 0;
static int      ehci_found        = 0;

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = (1U << 31)
                     | ((uint32_t)bus    << 16)
                     | ((uint32_t)device << 11)
                     | ((uint32_t)func   <<  8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t data) {
    uint32_t address = (1U << 31)
                     | ((uint32_t)bus    << 16)
                     | ((uint32_t)device << 11)
                     | ((uint32_t)func   <<  8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, data);
}

void scan_pci_device(uint8_t bus, uint8_t device) {
    uint32_t vendor_data = pci_read(bus, device, 0, 0x00);
    uint16_t vendor_id = vendor_data & 0xFFFF;
    if (vendor_id == 0xFFFF) return;

    uint32_t header_data = pci_read(bus, device, 0, 0x0C);
    uint8_t  header_type = (header_data >> 16) & 0xFF;
    uint8_t  func_limit  = (header_type & 0x80) ? 8 : 1;

    for (uint8_t func = 0; func < func_limit; func++) {
        uint32_t vd  = pci_read(bus, device, func, 0x00);
        uint16_t vid = vd & 0xFFFF;
        if (vid == 0xFFFF) continue;

        uint16_t did        = (vd >> 16) & 0xFFFF;
        uint32_t class_data = pci_read(bus, device, func, 0x08);
        uint8_t  class_code = (class_data >> 24) & 0xFF;
        uint8_t  subclass   = (class_data >> 16) & 0xFF;
        uint8_t  prog_if    = (class_data >>  8) & 0xFF;

        if (class_code == 0x01) {
            const char *type = "Unknown Storage";

            if (subclass == 0x01) {
                type = "IDE";
            } else if (subclass == 0x04) {
                type = "RAID";
            } else if (subclass == 0x06) {
                if (prog_if == 0x01) {
                    type = "SATA AHCI";
                    uint32_t  bar5      = pci_read(bus, device, func, 0x24);
                    uintptr_t ahci_base = (uintptr_t)(bar5 & 0xFFFFFFF0);
                    LOG_INFO("");
                    kprintf("AHCI MMIO base address: %lx\n", (unsigned long)ahci_base);
                    SERIAL(Info, scan_pci_device, "AHCI MMIO base address: %lx\n",
                           (unsigned long)ahci_base);
                    pid_rn = 7;
                    map_page((uint64_t)ahci_base, (uint64_t)ahci_base,
                             PTE_WRITABLE | PTE_CACHE_DISABLE | PTE_WRITE_THROUGH);
                    sata_search(ahci_base);
                } else {
                    type = "SATA (non-AHCI)";
                }
            }

            LOG_INFO("%s Controller: bus=%u dev=%u func=%u vendor id=%x dev id=%x\n",
                     type, bus, device, func, vid, did);
            SERIAL(Info, scan_pci_device,
                   "%s Controller: bus=%u dev=%u func=%u vendor id=%x dev id=%x\n",
                   type, bus, device, func, vid, did);

        } else if (class_code == 0x0C && subclass == 0x03) {
            const char *usb_type = "Unknown USB";

            if (prog_if == 0x00) {
                usb_type = "UHCI";
            } else if (prog_if == 0x10) {
                usb_type = "OHCI";
            } else if (prog_if == 0x20) {
                usb_type = "EHCI";
                LOG_INFO("EHCI Controller found: bus=%u dev=%u func=%u vendor=%x dev=%x — deferred\n",
                         bus, device, func, vid, did);
                SERIAL(Info, scan_pci_device,
                       "EHCI Controller found: bus=%u dev=%u func=%u — deferred\n",
                       bus, device, func);
                if (!ehci_found) {
                    ehci_saved_bus    = bus;
                    ehci_saved_device = device;
                    ehci_saved_func   = func;
                    ehci_found        = 1;
                }
            } else if (prog_if == 0x30) {
                usb_type = "xHCI";
            }

            LOG_INFO("%s Controller: bus=%u dev=%u func=%u vendor=%x dev=%x\n",
                     usb_type, bus, device, func, vid, did);
            SERIAL(Info, scan_pci_device,
                   "%s Controller: bus=%u dev=%u func=%u vendor=%x dev=%x\n",
                   usb_type, bus, device, func, vid, did);

        } else if (class_code == 0x06 && subclass == 0x04) {
            uint32_t bus_data     = pci_read(bus, device, func, 0x18);
            uint8_t  secondary_bus = (bus_data >> 8) & 0xFF;
            scan_pci_bus(secondary_bus);
        }
    }
}

void scan_pci_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        scan_pci_device(bus, device);
    }
}

void start_pci_enumeration(void) {
    uint32_t header_data = pci_read(0, 0, 0, 0x0C);
    uint8_t  header_type = (header_data >> 16) & 0xFF;

    if ((header_type & 0x80) == 0) {
        scan_pci_bus(0);
    } else {
        for (uint8_t func = 0; func < 8; func++) {
            uint32_t vd  = pci_read(0, 0, func, 0x00);
            uint16_t vid = vd & 0xFFFF;
            if (vid == 0xFFFF) continue;
            uint32_t bus_data = pci_read(0, 0, func, 0x18);
            uint8_t  bus_num  = (bus_data >> 8) & 0xFF;
            scan_pci_bus(bus_num);
        }
    }
}

void pci_get_ehci(uint8_t *bus, uint8_t *dev, uint8_t *func, int *found) {
    *bus   = ehci_saved_bus;
    *dev   = ehci_saved_device;
    *func  = ehci_saved_func;
    *found = ehci_found;
}

uint32_t pci_get_bar_size(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t orig = pci_read(bus, device, func, offset);
    pci_write(bus, device, func, offset, 0xFFFFFFFF);
    uint32_t size_mask = pci_read(bus, device, func, offset);
    pci_write(bus, device, func, offset, orig);
    size_mask &= ~0xF;
    return (~size_mask) + 1;
}

uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar_num) {
    if (bar_num > 5) return 0;
    uint8_t offset = 0x10 + (bar_num * 4);
    return pci_read(bus, device, func, offset);
}

void ehci_pci_init(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t bar0 = pci_read(bus, dev, func, 0x10);
    if (bar0 & 0x1) { LOG_FATAL("EHCI BAR0 is I/O\n"); return; }
    uint64_t phys = bar0 & 0xFFFFFFF0;
    uint32_t size = pci_get_bar_size(bus, dev, func, 0x10);
    for (uint64_t off = 0; off < size; off += 0x1000)
        map_page(phys + off, phys + off,
                 PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE | PTE_WRITE_THROUGH);
    ehci_mmio_phys = phys;
}

int module_init(void) {
    start_pci_enumeration();   

    return 0;
}