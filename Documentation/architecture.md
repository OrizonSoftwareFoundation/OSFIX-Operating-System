# Architecture

OSFIX currently boots as a 64-bit Limine-loaded kernel. Limine provides the
kernel image, framebuffer, physical memory map, higher-half direct map (HHDM),
RSDP pointer, and boot modules. The kernel then initializes core CPU state,
memory management, ACPI, a heap, the initramfs VFS root, and loadable modules.

## Boot Flow

The main entry point is `kmain` in `kernel/main/kernel.c`.

1. Validate that Limine returned at least one framebuffer.
2. Initialize Flanterm on the framebuffer for early terminal output.
3. Initialize serial logging.
4. Read CPU vendor and topology information with CPUID.
5. Install the GDT, IDT, and ISR handling.
6. Initialize physical memory management, virtual memory, ACPI, and FADT data.
7. Allocate a TLSF heap pool.
8. Initialize the PFN database.
9. Mount and unpack the Limine initramfs into RAMFS through the VFS.
10. Load priority modules from the initramfs.
11. Recursively load remaining `.elf` modules from the VFS root.

The current priority module order is:

```text
/apic/apic.elf
/storage/storage.elf
/pci/pci.elf
```

After priority modules are attempted, every other `.elf` file under `/` is
loaded recursively.

For the full sequence, see [Boot Process](boot-process.md).

## Bootloader Interface

The kernel uses Limine requests for:

- RSDP discovery.
- Physical memory map discovery.
- HHDM discovery.
- Framebuffer discovery.
- Boot module discovery for the initramfs.

The initramfs loader searches Limine modules whose path or metadata string
contains `initramfs`, then treats the module as a `newc` CPIO archive.

## Memory Model

The kernel is freestanding and uses a higher-half direct map for physical memory
access. Physical pages are allocated through PMM helpers and mapped through VMM
helpers. A TLSF pool is created early and exposed through the kernel allocation
functions used by kernel code and modules.

Important pieces:

- `kernel/main/mm/pmm.c`: physical page allocation and HHDM helpers.
- `kernel/main/mm/vmm.c`: page mapping and address translation helpers.
- `kernel/main/mm/pfndb.c`: physical frame database initialization.
- `kernel/main/mm/heapalloc/tlsf.c`: heap allocator implementation.
- `kernel/main/mm/heapalloc/malloc.c`: C allocation interface.

For details, see [Memory Management](memory-management.md).

## Filesystem Model

The kernel uses a VFS layer with pluggable filesystem operations. At boot,
`initramfs_init` registers RAMFS, mounts it as root, and unpacks the CPIO archive
into it. Later code can access files through VFS calls such as `vfs_read`,
`vfs_write`, `vfs_listdir`, and `vfs_path_to_inode`.

The VFS has fixed-size registries for filesystems and mounts. Mount path
selection currently recognizes root (`/`), `/proc`, and `/dev` based on
filesystem type values.

## Module Model

Loadable modules are built as relocatable ELF files and packed into the CPIO
initramfs. The kernel loads them from the VFS, applies supported x86_64
relocations, resolves symbols against kernel and module symbol tables, and calls
their `module_init` entry point.

Modules currently include:

- APIC interrupt controller support.
- Storage discovery.
- PCI enumeration.
- EHCI USB host controller support.
- exFAT filesystem support.

See [Modules](modules.md) for details.

## Documentation Map

Use these pages for subsystem-level detail:

- [Boot Process](boot-process.md)
- [Memory Management](memory-management.md)
- [Interrupts](interrupts.md)
- [Filesystems](filesystems.md)
- [Drivers](drivers.md)
- [Storage](storage.md)
- [exFAT](exfat.md)
- [Utilities](utilities.md)
- [Kernel API](kernel-api.md)
- [Troubleshooting](troubleshooting.md)
