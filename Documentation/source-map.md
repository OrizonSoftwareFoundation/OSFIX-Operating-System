# Source Map

This page is a human-written index of the main OSFIX source files. Use it when
you need to find the code behind a subsystem without generated API pages.

## Kernel Entry and Boot

- `kernel/main/kernel.c`: main kernel entry point and boot initialization order.
- `boot/limine.h`: Limine protocol declarations.
- `boot/assets/limine.conf`: bootloader configuration.
- `linker.ld`: kernel linker script.

Detailed docs: [Boot Process](boot-process.md).
Troubleshooting: [Troubleshooting](troubleshooting.md).

## CPU

- `kernel/main/cpu/gdt.c`: GDT setup.
- `kernel/main/cpu/idt.c`: IDT setup.
- `kernel/main/cpu/isr.c`: interrupt service routine setup.
- `kernel/main/cpu/isrs_gen.c`: generated ISR support code.
- `kernel/main/cpu/io.c`: port I/O helpers.
- `kernel/includes/main/cpu`: public CPU headers.
- `kernel/includes/main/cpu/asm/isr_stubs.asm`: ISR assembly stubs.

Detailed docs: [Interrupts](interrupts.md).

## Memory

- `kernel/main/mm/pmm.c`: physical page allocator and HHDM handling.
- `kernel/main/mm/vmm.c`: page-table mapping and address translation.
- `kernel/main/mm/pfndb.c`: page frame database.
- `kernel/main/mm/heapalloc/malloc.c`: allocation wrappers.
- `kernel/main/mm/heapalloc/tlsf.c`: TLSF allocator implementation.
- `kernel/includes/main/mm`: memory management headers.

Detailed docs: [Memory Management](memory-management.md).
API docs: [Kernel API](kernel-api.md).

## Modules and ELF

- `kernel/main/module/module.c`: exported kernel API table.
- `kernel/main/module/ksym.c`: kernel symbol lookup.
- `kernel/main/module/modsym.c`: module symbol lookup.
- `kernel/main/util/elf/elf_loader.c`: executable and module ELF loader.
- `kernel/includes/main/module`: module headers.
- `kernel/includes/main/util/elf`: ELF headers and loader declarations.

Detailed docs: [Modules](modules.md) and [Writing Modules](writing-modules.md).
API docs: [Kernel API](kernel-api.md).

## Filesystems and Initramfs

- `kernel/main/fsm/vfs.c`: VFS registry, mount table, and dispatch helpers.
- `kernel/main/fsm/initramfs/initramfs-unpacker.c`: Limine module and CPIO
  initramfs unpacker.
- `kernel/main/fsm/initramfs/ramfs.c`: in-memory root filesystem.
- `kernel/includes/main/fsm`: VFS and filesystem headers.
- `fs/exfat/exfat.c`: exFAT implementation.
- `fs/exfat/exfat_vfs.c`: exFAT VFS bridge.
- `fs/includes/exfat`: exFAT headers.

Detailed docs: [Filesystems](filesystems.md).
exFAT docs: [exFAT](exfat.md).

## ACPI

- `drivers/acpi/acpi.c`: RSDP discovery, RSDT/XSDT selection, table lookup.
- `drivers/acpi/acpi_shtdwn.c`: ACPI shutdown support.
- `drivers/includes/acpi/acpi.h`: ACPI structure and status declarations.
- `drivers/includes/acpi/acpi_fadt.h`: FADT declarations.
- `drivers/includes/acpi/acpi_shtdwn.h`: shutdown declarations.

Detailed docs: [Drivers](drivers.md).

## Interrupt Controllers

- `drivers/pic/apic/apic.c`: APIC setup.
- `drivers/pic/apic/apic_irq.c`: APIC IRQ support.
- `drivers/includes/pic/apic`: APIC headers.

## PCI and Devices

- `drivers/pci/pci.c`: PCI configuration access, bus scanning, controller
  detection, AHCI/EHCI discovery.
- `drivers/includes/pci/pci.h`: PCI declarations.

Detailed docs: [Drivers](drivers.md).
Storage docs: [Storage](storage.md).

## Storage

- `drivers/storage/ata.c`: ATA probing.
- `drivers/storage/atapi.c`: ATAPI probing.
- `drivers/storage/sata.c`: SATA/AHCI support.
- `drivers/storage/stmodule.c`: storage module initialization.
- `drivers/includes/storage`: storage headers.

## USB

- `drivers/hci/hci.c`: EHCI host controller initialization and transfer helpers.
- `drivers/includes/hci/hci.h`: HCI declarations and register structures.

## Terminal, Logging, and Utilities

- `kernel/main/terminal/src`: Flanterm terminal backend.
- `kernel/main/util/print/kprintf.c`: formatted kernel printing.
- `kernel/main/util/print/log.c`: log helpers.
- `kernel/main/util/print/serial.c`: serial output.
- `kernel/main/util/string.c`: string helpers.
- `kernel/main/util/math.c`: math helpers.
- `kernel/main/util/util.c`: miscellaneous utilities.
- `kernel/includes/main/util`: utility headers.

## Time

- `kernel/main/util/time/tsc.c`: TSC support.
- `kernel/main/util/time/pit.c`: PIT support.
- `kernel/main/util/time/ktime.c`: kernel timing helpers.
- `kernel/includes/main/util/time`: time headers.

Detailed docs: [Utilities](utilities.md).
