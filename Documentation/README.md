# OSFIX Documentation

This directory contains the hand-written documentation for OSFIX.

OSFIX is an x86_64 hobby operating system built around the Limine bootloader,
a freestanding C kernel, a small VFS, a CPIO-backed initramfs, and loadable ELF
modules for hardware and filesystem support.

## Contents

- [Architecture](architecture.md) explains the boot flow and major kernel
  subsystems.
- [Boot Process](boot-process.md) documents the exact early initialization
  sequence.
- [Building and Running](building-and-running.md) lists dependencies and common
  build commands.
- [Memory Management](memory-management.md) documents PMM, VMM, PFNDB, and heap
  allocation.
- [Kernel API](kernel-api.md) lists the main public kernel interfaces by
  subsystem.
- [Interrupts](interrupts.md) covers GDT, IDT, ISR, APIC, and panic handling.
- [Modules](modules.md) documents how initramfs modules are built, loaded, and
  initialized.
- [Writing Modules](writing-modules.md) is a practical guide for adding a new
  loadable module.
- [Filesystems](filesystems.md) covers VFS, RAMFS, initramfs, and exFAT.
- [exFAT](exfat.md) documents the exFAT implementation and VFS bridge in more
  detail.
- [Drivers](drivers.md) covers ACPI, PCI, APIC, storage, SATA, ATA, ATAPI, and
  EHCI USB.
- [Storage](storage.md) documents ATA, ATAPI, SATA/AHCI, storage devices, and
  disk callbacks.
- [Utilities](utilities.md) covers logging, printing, serial, timing, strings,
  math, and conversion helpers.
- [Troubleshooting](troubleshooting.md) maps common boot and runtime symptoms to
  likely causes.
- [Subsystems](subsystems.md) summarizes ACPI, memory management, VFS,
  filesystems, PCI, storage, USB, terminal, logging, and time support.
- [Developer Notes](developer-notes.md) records conventions, known sharp edges,
  and safe extension points.
- [Source Map](source-map.md) points to the important source files and public
  headers.

## Quick Start

From the repository root:

```sh
make
make run
```

These docs are plain Markdown files. No documentation generator is required.
