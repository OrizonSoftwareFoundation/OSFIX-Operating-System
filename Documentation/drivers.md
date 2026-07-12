# Drivers

This page documents hardware-related code in `drivers` and filesystem/device
modules.

For storage-specific details, see [Storage](storage.md).

## ACPI

ACPI table discovery lives in `drivers/acpi/acpi.c`.

The ACPI path supports:

- RSDP discovery from Limine or BIOS memory scanning.
- RSDP checksum validation.
- XSDT selection for ACPI 2.0+ when available.
- RSDT fallback.
- SDT checksum validation.
- Table lookup by four-character signature.

`acpi_parse_fadt` in `drivers/acpi/acpi_shtdwn.c` finds the FADT, chooses PM1
control ports, and stores DSDT AML for later shutdown use.

`acpi_shutdown` searches the DSDT AML for `_S5_`, extracts sleep type values,
sets `SLP_EN`, and writes PM1A and PM1B control ports.

Current limitation: ACPI shutdown only handles System I/O PM1 control blocks,
not memory-mapped PM1 control blocks.

## PCI

PCI code lives in `drivers/pci/pci.c`.

It uses configuration ports:

- `0xCF8` for address.
- `0xCFC` for data.

The scanner walks buses, devices, and functions. It handles multifunction
devices and PCI-to-PCI bridges.

Recognized devices:

- IDE, RAID, SATA AHCI, and non-AHCI SATA storage controllers.
- UHCI, OHCI, EHCI, and xHCI USB controllers.
- PCI bridges.

When AHCI is found, BAR5 is mapped with cache disabled and write-through flags,
then `sata_search` is called.

When EHCI is found, its bus/device/function are saved for deferred HCI setup.

## Storage Module

The storage module entry point is in `drivers/storage/stmodule.c`.

It probes:

- Two ATA drive slots.
- Two ATAPI drive slots.
- SATA, if `sata_get_port` reports a discovered port.

Discovered devices are stored in `storage_devices`, with `storage_device_count`
tracking the number of entries.

Device type identifiers:

- `P`: ATA/PATA.
- `Q`: ATAPI.
- `S`: SATA.

More detail: [Storage](storage.md).

## ATA and ATAPI

ATA and ATAPI code lives in:

- `drivers/storage/ata.c`
- `drivers/storage/atapi.c`
- `drivers/includes/storage/ata.h`
- `drivers/includes/storage/atapi.h`

ATA supports 28-bit and 48-bit LBA read/write paths and identify commands.
ATAPI supports identify and READ(10)-style sector reads using reported sector
size.

## SATA and AHCI

SATA declarations are in `drivers/includes/storage/sata.h`, with implementation
in `drivers/storage/sata.c`.

Supported operations include:

- AHCI read and write.
- AHCI identify.
- SATAPI identify and read helpers.
- AHCI port search.
- Global LBA read/write helpers used by exFAT fallback paths.

PCI maps AHCI MMIO before asking SATA to scan.

## EHCI USB

EHCI code lives in `drivers/hci/hci.c` and `drivers/includes/hci/hci.h`.

The EHCI layer contains:

- Register structures for capability and operational registers.
- Queue head and qTD structures.
- Controller stop and reset routines.
- Async list initialization.
- Port power, connection, and reset helpers.
- Simple bulk transfer submission.
- USB control transfer helpers.
- Device and configuration descriptor reads.
- Address and configuration setup.
- USB mass-storage reset, CBW, CSW, and READ(10) helpers.

The controller uses physical pages from PMM for queue structures and transfer
buffers, then translates through HHDM and flushes caches where needed.

## APIC

APIC is documented in [Interrupts](interrupts.md). It is implemented under
`drivers/pic/apic` and loaded as `/apic/apic.elf`.

## Driver Ordering

Current boot module priority is:

1. APIC.
2. Storage.
3. PCI.

Then all remaining modules are loaded recursively from RAMFS. Dependencies are
mostly manual, so new drivers that depend on another module should either be
loaded after it by path order or added to the priority list in `kmain`.

## Related Pages

- [Storage](storage.md)
- [Interrupts](interrupts.md)
- [Kernel API](kernel-api.md)
- [Troubleshooting](troubleshooting.md)
