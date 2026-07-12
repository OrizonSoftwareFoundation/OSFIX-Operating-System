# Building and Running

OSFIX is built with GNU Make and a freestanding C toolchain. The main outputs
are the kernel ELF, the initramfs CPIO archive, and a UEFI disk image.

## Dependencies

Install the equivalents of these packages for your distribution:

- GNU Make.
- A C compiler available as `cc`.
- `ld`.
- NASM.
- QEMU for x86_64.
- OVMF or another UEFI firmware image supported by QEMU.
- `parted`.
- `dosfstools`.
- `mtools`.
- `cpio`.

## Common Commands

Build the kernel and initramfs:

```sh
make
```

Build the disk image:

```sh
make image
```

Run in QEMU:

```sh
make run
```

Run with the AMD-flavored QEMU CPU option:

```sh
make run-amd
```

Clean generated build products:

```sh
make clean
```

## Build Outputs

The default build creates:

- `build/kernel.elf`: the linked kernel.
- `build/initramfs.cpio`: the CPIO archive containing module ELFs.
- `OSFIX.img`: the UEFI disk image created by `make image`.

## Build Layout

Kernel sources are compiled into `build/` with freestanding x86_64 flags.
Driver and filesystem modules listed in the `MODULES` variable are compiled with
large-code-model flags and linked with `ld -r` as relocatable ELF files.

The module archive layout is:

```text
build/initramfs/<module-name>/<module-name>.elf
```

The generated CPIO archive preserves those paths so the kernel can load modules
such as `/pci/pci.elf` after unpacking the initramfs.

## Documentation

The documentation is regular Markdown in `Documentation`. Read it directly from
the repository; no generator is required.
