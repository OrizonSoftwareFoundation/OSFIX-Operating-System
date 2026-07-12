#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8

#define PCI_CONFIG_DATA    0xCFC

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);

void pci_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t data);

void scan_pci_bus(uint8_t bus);

uint32_t pci_get_bar_size(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);

uint32_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t func, uint8_t bar_num);

void start_pci_enumeration(void);

#endif
