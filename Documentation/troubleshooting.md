# Troubleshooting

This page maps common symptoms to likely causes in the current OSFIX codebase.

## Nothing Appears on Screen

Likely causes:

- Limine did not provide a framebuffer.
- Flanterm initialization failed or received bad framebuffer data.
- QEMU is running without the expected display backend.

Where to look:

- `kernel/main/kernel.c`
- `kernel/main/terminal/src`
- `boot/assets/limine.conf`

Try serial output first, because serial initializes early and is often easier to
inspect than framebuffer output.

## Boot Stops Before PMM

Likely causes:

- Missing Limine HHDM response.
- Missing Limine memory map response.
- No usable memory region large enough for the PMM bitmap.

Where to look:

- `kernel/main/mm/pmm.c`
- Limine memory map logging on serial.

## Heap Allocation Fails

Likely causes:

- `palloc_order(10)` failed.
- HHDM translation is wrong.
- TLSF pool creation failed.

Where to look:

- `kernel/main/kernel.c`
- `kernel/main/mm/pmm.c`
- `kernel/main/mm/heapalloc/tlsf.c`

The kernel currently halts on heap allocation failure.

## Initramfs Does Not Load

Likely causes:

- Limine did not provide a module containing `initramfs` in path or string.
- The CPIO archive is not `newc`.
- CPIO alignment or magic is wrong.
- RAMFS mount failed.

Where to look:

- `kernel/main/fsm/initramfs/initramfs-unpacker.c`
- `kernel/main/fsm/initramfs/ramfs.c`
- `boot/assets/limine.conf`
- `build/initramfs.cpio`

Expected CPIO magic is:

```text
070701
```

## Module Is Not Loaded

Likely causes:

- The module was not added to `MODULES`.
- The module did not produce a `.elf` under `build/initramfs/<name>`.
- The initramfs archive did not include the module.
- The recursive loader did not see the path.

Where to look:

- Root Makefile `MODULES`.
- `build/initramfs`.
- `kernel/main/kernel.c`.

## Module Load Fails

Likely causes:

- Invalid ELF file.
- Undefined symbol.
- Unsupported relocation.
- Allocation failure.
- Dependency module loaded too late.

Where to look:

- `kernel/main/util/elf/elf_loader.c`
- `kernel/main/module/ksym.c`
- `kernel/main/module/modsym.c`
- Serial output for `module:` messages.

## Page Fault or Panic

Likely causes:

- Invalid pointer.
- Bad physical-to-virtual conversion.
- Missing page mapping.
- MMIO accessed before mapping.
- Stack corruption.

The panic path prints:

- interrupt number
- RIP
- mode
- CR2 for page faults
- error code
- stack trace guess
- general registers
- control registers
- debug registers
- GDT and IDT base/limit

Where to look:

- `kernel/main/cpu/isr.c`
- the faulting RIP from serial output

## ACPI Table Not Found

Likely causes:

- RSDP was not found.
- XSDT or RSDT checksum failed.
- The requested signature is not present.

Where to look:

- `drivers/acpi/acpi.c`

For shutdown failures, also check:

- `drivers/acpi/acpi_shtdwn.c`
- whether FADT was parsed
- whether `_S5_` exists in DSDT AML

## SATA or exFAT Fails

Likely causes:

- PCI did not find AHCI.
- AHCI BAR was not mapped.
- `sata_search` found no usable port.
- exFAT boot sector validation failed.
- exFAT is trying to use SATA fallback with no global SATA port.

Where to look:

- `drivers/pci/pci.c`
- `drivers/storage/sata.c`
- `fs/exfat/exfat.c`

## USB/EHCI Fails

Likely causes:

- PCI found EHCI but HCI initialized too early or too late.
- MMIO BAR mapping is wrong.
- Controller did not halt or reset before timeout.
- Port ownership was handed to another controller.
- Async queue structures are not mapped or flushed correctly.

Where to look:

- `drivers/pci/pci.c`
- `drivers/hci/hci.c`

## Timing Returns Zero

Likely causes:

- CPU clock speed calibration failed.
- `boot_tsc` was not set.
- TSC is unavailable.
- PIT legitimacy check failed.

Where to look:

- `kernel/main/util/time/ktime.c`
- `kernel/main/util/time/tsc.c`
- `kernel/main/util/time/pit.c`

## Documentation Looks Out of Date

Update:

- The specific subsystem page.
- [Source Map](source-map.md).
- [Kernel API](kernel-api.md), if a public header changed.
- [Troubleshooting](troubleshooting.md), if the failure mode is common.
