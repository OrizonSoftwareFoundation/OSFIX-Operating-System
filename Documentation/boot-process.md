# Boot Process

This page documents the current OSFIX boot path as implemented in
`kernel/main/kernel.c` and the initramfs loader.

## Limine Requests

The kernel uses Limine requests for:

- Framebuffer discovery.
- Memory map discovery.
- Higher-half direct map discovery.
- RSDP discovery.
- Boot module discovery for the initramfs.

The framebuffer request is checked first. If no framebuffer exists, `kmain`
jumps to the halt loop.

## Early Console

After the framebuffer is accepted, the kernel initializes Flanterm with:

- The first Limine framebuffer.
- Background color `0x00282828`.
- Foreground color `0x00ebdbb2`.

Serial logging is initialized immediately after the framebuffer terminal. Most
later boot code logs to both terminal and serial through `LOG_*` and `SERIAL`.

## CPU Discovery

The kernel reads CPUID vendor data and logs:

- CPU vendor string.
- Logical processor count from CPUID leaf 1.
- Physical core estimate using Intel leaf 4 or AMD leaf `0x80000008`.

Unsupported vendors fall back to one physical core.

## Core Initialization Order

The current `kmain` sequence is:

1. Record `boot_tsc` with `read_tsc_fast`.
2. Initialize terminal output.
3. Initialize serial output.
4. Read CPU vendor and core information.
5. Initialize the GDT.
6. Initialize the IDT.
7. Calibrate CPU clock speed.
8. Initialize ISR gates and handlers.
9. Initialize physical memory management.
10. Initialize virtual memory management.
11. Initialize ACPI from Limine RSDP or by scanning.
12. Parse the ACPI FADT.
13. Allocate a 4 MiB TLSF heap pool from PMM pages.
14. Initialize the PFN database from the Limine memory map.
15. Test a small heap allocation.
16. Initialize and unpack the initramfs.
17. Load priority modules.
18. Recursively load remaining `.elf` modules from RAMFS.
19. Enter the final halt loop.

## Initramfs Flow

`initramfs_init` registers RAMFS, mounts it as the VFS root, then searches
Limine modules for a path or string containing `initramfs`.

Matching modules are treated as `newc` CPIO archives. The unpacker:

- Validates the `070701` CPIO magic.
- Converts CPIO hex fields.
- Normalizes paths by stripping leading `/`.
- Creates RAMFS directories and files.
- Writes regular file payloads into RAMFS.
- Stops on `TRAILER!!!`.

The archive data is converted through HHDM if Limine provided a physical
address.

## Module Loading Flow

After the initramfs is unpacked, the kernel loads:

```text
/apic/apic.elf
/storage/storage.elf
/pci/pci.elf
```

Then it recursively scans `/` in the mounted RAMFS and loads any other `.elf`
files. A path table prevents loading the same module path twice.

## Failure Behavior

Some boot failures halt immediately:

- Missing framebuffer.
- Missing HHDM.
- Missing memory map.
- Failed PMM bitmap placement.
- Failed page table allocation.
- Failed heap allocation.

Other failures are logged and boot continues:

- Individual module load failure.
- Missing ACPI table lookups.
- Initramfs file or directory creation failures.
