# Modules

OSFIX modules are relocatable ELF objects stored in the initramfs. They provide
hardware and filesystem functionality while keeping the kernel core smaller.

For a practical step-by-step guide to adding one, see
[Writing Modules](writing-modules.md).

## Build Process

Modules are declared in the root Makefile as `name:source_dir` entries. The
current module list is:

```text
apic:drivers/pic/apic
storage:drivers/storage
pci:drivers/pci
hci:drivers/hci
exfat:fs/exfat
```

Each module source directory is compiled into objects under `build/`, then
linked with:

```sh
ld -r -m elf_x86_64
```

The result is placed in:

```text
build/initramfs/<name>/<name>.elf
```

All module ELFs are packed into `build/initramfs.cpio`.

## Loading Process

At boot, the kernel:

1. Mounts RAMFS as the root VFS.
2. Unpacks the CPIO initramfs into RAMFS.
3. Loads selected priority module paths.
4. Recursively scans the VFS root and loads every remaining `.elf`.

The loader prevents duplicate loads by remembering paths already attempted
successfully.

## Entry Point

A module should export:

```c
int module_init(void);
```

The kernel currently calls the entry point as a `module_init_fn`. The typedef in
`kernel/includes/main/module/module.h` accepts a `const kernel_api_t *api`
parameter, but existing modules use `int module_init(void)`, and the current
loader calls `init(NULL)`.

Treat the API pointer as unavailable until the loader and module entry
convention are unified.

## Symbol Resolution

The module loader resolves undefined symbols against:

- The kernel symbol table.
- Previously registered module symbols.

This allows modules to use selected kernel facilities such as logging,
allocation, memory helpers, PCI helpers, and storage interfaces when those
symbols are exported.

## Relocations

Modules are built with `-mcmodel=large` because they may be loaded at arbitrary
addresses. This avoids relying on PC-relative 32-bit relocations that cannot
always reach from the loaded module image to kernel symbols.

The loader supports a subset of x86_64 relocation records. Unsupported
relocations fail the load.

## Adding a Module

1. Create a source directory under the appropriate subsystem.
2. Add a `module_init` function.
3. Append `name:path` to the root Makefile `MODULES` list.
4. Ensure any needed symbols are available through the kernel or a dependency
   module.
5. Build and boot with serial logging enabled to confirm the module loads.

Because the kernel loads priority modules before recursively loading the rest,
add a module to the priority list in `kmain` only when another module needs it
to exist first.

## Current Limitations

- The declared `kernel_api_t` table is not passed to modules yet.
- `kernel_api_t.ksleep_ms` is declared but not initialized in the current
  `kernel_api` instance.
- Module dependency ordering is mostly implicit.
- Failed module loads are logged, but there is no dependency-aware retry pass.

## Related Pages

- [Kernel API](kernel-api.md)
- [Writing Modules](writing-modules.md)
- [Troubleshooting](troubleshooting.md)
