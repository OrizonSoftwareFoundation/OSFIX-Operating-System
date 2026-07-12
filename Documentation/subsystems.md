# Subsystems

This page summarizes the main OSFIX subsystems and where to find their code.

## ACPI

ACPI discovery lives in `drivers/acpi`. The kernel first prefers Limine's RSDP
response and falls back to scanning the EBDA and BIOS area if needed.

Supported flow:

- Validate the RSDP checksum.
- Prefer XSDT on ACPI 2.0+ systems.
- Fall back to RSDT.
- Validate table checksums before returning tables.
- Parse FADT data after ACPI initialization.

Key files:

- `drivers/acpi/acpi.c`
- `drivers/acpi/acpi_shtdwn.c`
- `drivers/includes/acpi/acpi.h`
- `drivers/includes/acpi/acpi_fadt.h`

More detail: [Drivers](drivers.md).

## CPU and Interrupts

CPU setup includes GDT, IDT, and ISR initialization. APIC support is built as a
module and loaded early.

Key files:

- `kernel/main/cpu/gdt.c`
- `kernel/main/cpu/idt.c`
- `kernel/main/cpu/isr.c`
- `kernel/includes/main/cpu/asm/isr_stubs.asm`
- `drivers/pic/apic`

More detail: [Interrupts](interrupts.md).

## Memory Management

Physical and virtual memory are initialized before ACPI and heap setup. The heap
uses TLSF over a pool allocated from physical pages and mapped through the HHDM.

Key files:

- `kernel/main/mm/pmm.c`
- `kernel/main/mm/vmm.c`
- `kernel/main/mm/pfndb.c`
- `kernel/main/mm/heapalloc/tlsf.c`
- `kernel/main/mm/heapalloc/malloc.c`

More detail: [Memory Management](memory-management.md).

## VFS and Initramfs

The VFS provides a filesystem operation table and mount records. The initramfs
loader mounts RAMFS and unpacks a Limine-provided `newc` CPIO archive into it.

Key files:

- `kernel/main/fsm/vfs.c`
- `kernel/includes/main/fsm/vfs.h`
- `kernel/main/fsm/initramfs/initramfs-unpacker.c`
- `kernel/main/fsm/initramfs/ramfs.c`

More detail: [Filesystems](filesystems.md).

## ELF Loading

The ELF loader supports executable loading and relocatable module loading.
Executable loading maps PT_LOAD segments and prepares a stack. Module loading
copies allocated sections, resolves symbols, applies supported relocations, and
returns the module entry point.

Key files:

- `kernel/main/util/elf/elf_loader.c`
- `kernel/includes/main/util/elf/elf_loader.h`
- `kernel/includes/main/util/elf/elf.h`

## PCI

The PCI module scans buses, devices, and functions through PCI configuration I/O
ports. It recognizes storage controllers, USB controllers, bridges, AHCI BARs,
and EHCI controller location for later USB initialization.

Key files:

- `drivers/pci/pci.c`
- `drivers/includes/pci/pci.h`

More detail: [Drivers](drivers.md).

## Storage

The storage module probes legacy ATA and ATAPI devices and records basic device
metadata. It also registers a SATA device when AHCI discovery finds a usable
port.

Key files:

- `drivers/storage/ata.c`
- `drivers/storage/atapi.c`
- `drivers/storage/sata.c`
- `drivers/storage/stmodule.c`
- `drivers/includes/storage`

More detail: [Drivers](drivers.md).

## USB HCI

The HCI module currently focuses on EHCI. It can stop and reset the controller,
initialize the asynchronous schedule, power and reset ports, and submit simple
bulk transfers.

Key files:

- `drivers/hci/hci.c`
- `drivers/includes/hci/hci.h`

More detail: [Drivers](drivers.md).

## Filesystems

RAMFS is used for the initramfs root. exFAT exists as a loadable filesystem
module with VFS integration code.

Key files:

- `kernel/main/fsm/initramfs/ramfs.c`
- `fs/exfat/exfat.c`
- `fs/exfat/exfat_vfs.c`

More detail: [Filesystems](filesystems.md).

## Terminal and Logging

Framebuffer terminal output uses Flanterm. Serial output and log macros are used
throughout early kernel and module code for diagnostics.

Key files:

- `kernel/main/terminal/src`
- `kernel/main/util/print/kprintf.c`
- `kernel/main/util/print/log.c`
- `kernel/main/util/print/serial.c`

## Time

Time helpers include TSC, PIT, and kernel time routines. These are used for
delays, controller timeouts, and boot diagnostics.

Key files:

- `kernel/main/util/time/tsc.c`
- `kernel/main/util/time/pit.c`
- `kernel/main/util/time/ktime.c`

More detail: [Utilities](utilities.md).
