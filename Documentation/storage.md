# Storage

This page documents the current storage stack in more detail than the general
driver overview.

## Layers

The storage stack is split across:

- ATA and ATAPI protocol code.
- SATA/AHCI protocol code.
- Storage module probing.
- Generic storage device structures.
- Disk callbacks used by filesystems.

Important files:

- `drivers/storage/ata.c`
- `drivers/storage/atapi.c`
- `drivers/storage/sata.c`
- `drivers/storage/stmodule.c`
- `drivers/includes/storage/storage.h`
- `drivers/includes/storage/ata.h`
- `drivers/includes/storage/atapi.h`
- `drivers/includes/storage/sata.h`

## Storage Device Types

`storage_device_t` describes a generic device with:

- `type_identifier`
- protocol-specific union data
- sector size
- capacity

The legacy module also uses `legacy_storage_device_t`:

- `type_identifier`
- `device_data`
- `sector_size`

Current type identifiers:

- `P`: ATA/PATA.
- `Q`: ATAPI.
- `S`: SATA.

`storage_devices` can hold up to `MAX_STORAGE_DEVICES`, currently 1028.

## ATA

ATA code supports:

- 28-bit LBA reads.
- 48-bit LBA reads.
- Automatic read path selection.
- 28-bit and 48-bit writes.
- Cache flush.
- Identify commands.
- Manual PMIO-base variants.

ATA is probed for drive numbers 0 and 1 by the storage module.

## ATAPI

ATAPI code supports:

- Sector reads using packet commands.
- Manual PMIO-base reads.
- Identify commands.
- READ CAPACITY-style useful identify data through
  `identify_atapi_useful`.

ATAPI is probed for drive numbers 0 and 1 by the storage module.

## SATA/AHCI

SATA/AHCI code supports:

- AHCI read sectors.
- AHCI write sectors.
- AHCI identify.
- SATAPI-on-AHCI read and identify helpers.
- AHCI port scan.
- Global `sata_read_lba` and `sata_write_lba` helpers.
- `sata_get_port` for the currently discovered port.

PCI scans for AHCI controllers. When one is found, it maps BAR5 as MMIO and
calls `sata_search`.

## Disk Callbacks

`storage.h` declares callback-style disk access:

```c
int disk_read_callback(void *device, uint64_t sector, uint32_t count, void *buffer);
int disk_write_callback(void *device, uint64_t sector, uint32_t count, const void *buffer);
```

Filesystems can use these callbacks when they receive a device pointer. exFAT
uses callback access when `device` is non-null and falls back to global SATA LBA
helpers when `device` is null.

## AHCI Data Structures

`sata.h` declares structures for:

- AHCI port memory.
- Host-to-device register FIS.
- Command headers.
- PRDT entries.
- Command tables.

These structures are packed around AHCI hardware layout expectations. Be careful
with alignment and physical address conversion when modifying command setup.

## Current Limitations

- The storage module probes a small fixed set of legacy ATA/ATAPI drive slots.
- SATA global read/write helpers assume one discovered global port.
- There is no full partition scanning layer documented in active boot use yet.
- Device dependency ordering with PCI is fragile because storage currently loads
  before PCI in the priority list.
