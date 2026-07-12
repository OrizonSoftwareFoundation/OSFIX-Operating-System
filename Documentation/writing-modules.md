# Writing Modules

This guide describes how to add a new OSFIX loadable module.

## When to Use a Module

Use a module for code that can initialize after:

- PMM and VMM are initialized.
- The heap is available.
- RAMFS is mounted.
- The initramfs is unpacked.
- The ELF module loader is available.

Good module candidates include hardware drivers, filesystem drivers, and
optional kernel services. Keep core boot requirements in the kernel itself.

## Minimal Module

A current module exports `module_init`:

```c
#include <log.h>

int module_init(void) {
    LOG_INFO("my module loaded\n");
    return 0;
}
```

Existing modules use `int module_init(void)`. The `module_init_fn` typedef in
`module.h` takes a `const kernel_api_t *`, but `kmain` currently calls module
entry points with `NULL`. Do not depend on a module API pointer yet.

## Build Placement

Create a source directory, for example:

```text
drivers/mydriver/mydriver.c
```

Then add the module to the root Makefile `MODULES` list:

```make
MODULES := \
    apic:drivers/pic/apic  \
    storage:drivers/storage \
    pci:drivers/pci         \
    hci:drivers/hci         \
    exfat:fs/exfat          \
    mydriver:drivers/mydriver
```

The build system creates:

```text
build/initramfs/mydriver/mydriver.elf
```

The initramfs pack step puts that ELF into `build/initramfs.cpio`.

## Compiler Model

Modules are compiled with `DRIVER_CFLAGS`, including:

- `-mcmodel=large`
- `-mno-red-zone`
- `-fno-pic`
- `-fno-pie`
- `-mno-sse`
- `-mno-sse2`

The large code model matters because the loader can place modules at arbitrary
addresses. Avoid assumptions about short relative jumps to kernel symbols.

## Include Paths

The root Makefile already includes common paths:

- `kernel`
- `kernel/main`
- `kernel/includes/main`
- `kernel/includes/main/util`
- `kernel/includes/main/util/print`
- `drivers`
- `drivers/includes`
- `drivers/includes/storage`
- `drivers/includes/pci`
- `drivers/includes/hci`

Prefer existing headers and helper APIs. Do not include source files directly.

## Symbol Use

Modules resolve undefined symbols through kernel and module symbol tables. If a
module fails to load with an undefined symbol, either:

- Include the correct header and ensure the function exists in linked kernel
  code.
- Export or register the needed symbol if the project symbol table requires it.
- Move dependency code into a module that loads earlier.

## Dependency Ordering

Current priority module paths are hard-coded in `kmain`:

```text
/apic/apic.elf
/storage/storage.elf
/pci/pci.elf
```

Everything else is loaded by recursive directory scan. If a new module must
load before another module, either place it in the priority list or remove the
ordering dependency.

## Module Logging

Use logs during initialization:

```c
LOG_OK("driver initialized\n");
LOG_WARN("optional device missing\n");
SERIAL(Info, module_init, "raw serial details\n");
```

Prefer returning nonzero from `module_init` when initialization fails in a way
callers should care about. The current loader logs the load result, calls init,
and does not yet act on the init return value.

## Common Failure Modes

- `undefined symbol`: module references a function not visible to the loader.
- unsupported relocation: code generation produced relocation types the loader
  does not handle.
- module never runs: it was not added to `MODULES`, or the initramfs did not
  pack it.
- dependency unavailable: the module loaded before the module it expects.
- crash in `module_init`: most likely use of uninitialized hardware state,
  invalid MMIO mapping, or heap use before boot heap setup.

## Checklist

Before considering a module ready:

- It builds into `build/initramfs/<name>/<name>.elf`.
- It logs a clear success or failure message.
- It tolerates missing hardware when appropriate.
- It does not assume the API pointer is valid.
- It uses PMM/VMM helpers for physical pages and MMIO.
- It documents any required load ordering.
