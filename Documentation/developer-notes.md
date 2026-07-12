# Developer Notes

These notes capture current project conventions and places where future changes
should be made carefully.

## Code Style

- The codebase is freestanding C with x86_64-specific assumptions.
- Headers live under subsystem-specific `includes` directories.
- Logging through `LOG_*` and `SERIAL` is common during boot and module init.
- Modules should keep initialization failures visible through logs.

## Safe Extension Points

Add kernel core functionality when it must exist before modules load, such as
memory management, CPU state, bootloader handoff, or initramfs setup.

Add module functionality when the feature can initialize after the root RAMFS is
available, such as hardware drivers and filesystems.

Add VFS filesystems by implementing `vfs_ops_t`, registering a filesystem type,
and mounting it through `vfs_mount`.

## Initialization Order Matters

Several subsystems depend on earlier initialization:

- Logging expects terminal or serial setup.
- ACPI depends on valid physical-to-virtual access.
- Heap setup depends on PMM and HHDM.
- Initramfs depends on VFS and RAMFS.
- Module loading depends on initramfs contents and ELF symbol resolution.
- HCI depends on PCI discovering or exposing controller MMIO data.

When adding new boot-time code, place it after its dependencies are initialized
and before the first subsystem that needs it.

## Documentation Policy

Keep handwritten documentation in this directory. The project documentation is
plain Markdown and should stay readable directly in an editor or repository
viewer.

When a public header changes, update the relevant Markdown page and the source
map.

When a subsystem behavior changes, update its detailed page:

- Boot order: [Boot Process](boot-process.md)
- Memory allocation or mapping: [Memory Management](memory-management.md)
- Interrupt handling: [Interrupts](interrupts.md)
- VFS, RAMFS, initramfs, or exFAT: [Filesystems](filesystems.md)
- Hardware support: [Drivers](drivers.md)
- Logging, printing, timing, or libc-style helpers: [Utilities](utilities.md)
- Public interfaces: [Kernel API](kernel-api.md)
- Module implementation: [Writing Modules](writing-modules.md)
- Storage internals: [Storage](storage.md)
- exFAT internals: [exFAT](exfat.md)
- Failure diagnosis: [Troubleshooting](troubleshooting.md)

## Known Sharp Edges

- The root Makefile currently advertises `docs` as a phony target, but does not
  define a `docs` recipe.
- The top-level README mentions generated docs, but this documentation set is
  plain Markdown.
- `kernel_api_t` and the actual module entry convention are not fully aligned.
- `kernel_api_t.ksleep_ms` is declared but not initialized in `kernel_api`.
- Module dependency ordering is implicit outside the hard-coded priority list.
- VFS filesystem and mount registries are fixed-size arrays.
- The initramfs loader accepts the first Limine module whose path or string
  contains `initramfs`.
- RAMFS directory listing is root-only in the current implementation.
- ACPI shutdown only handles System I/O PM1 control blocks.
- PIT/TSC delays are implemented; HPET, APIC, IOAPIC, and PIC delay modes are
  placeholders.

## Review Checklist

Before merging subsystem changes:

- Confirm the build still produces `build/kernel.elf`.
- Confirm module ELFs appear under `build/initramfs`.
- Boot with QEMU and watch serial output for initialization failures.
- Check that new public interfaces are documented here and listed in the source
  map when useful.
- Avoid relying on host C library behavior unavailable in freestanding mode.
