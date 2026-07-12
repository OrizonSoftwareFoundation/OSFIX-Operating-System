# exFAT

The exFAT implementation provides both raw filesystem operations and a VFS
adapter.

## Files

- `fs/exfat/exfat.c`: core exFAT logic.
- `fs/exfat/exfat_vfs.c`: VFS adapter.
- `fs/includes/exfat/exfat.h`: on-disk structures and public functions.
- `fs/includes/exfat/exfat_vfs.h`: registration function.

## On-Disk Structures

The implementation defines packed structures for:

- Boot sector.
- Generic directory entry.
- File directory entry.
- Stream extension entry.
- File name entry.

Important directory entry types:

- `EXFAT_TYPE_EOD`: end of directory.
- `EXFAT_TYPE_ALLOCATION`: allocation bitmap.
- `EXFAT_TYPE_UPCASE`: upcase table.
- `EXFAT_TYPE_VOLUME_LABEL`: volume label.
- `EXFAT_TYPE_FILE`: file entry.
- `EXFAT_TYPE_STREAM`: stream extension.
- `EXFAT_TYPE_NAME`: name entry.

Important FAT values:

- `EXFAT_FAT_FREE`
- `EXFAT_FAT_BAD`
- `EXFAT_FAT_EOC`

## Mounting

`exfat_mount` reads sector zero, validates:

- `fs_name == "EXFAT   "`
- boot signature `0xAA55`

It then fills:

- bytes per sector
- sectors per cluster
- bytes per cluster
- FAT offset and length
- cluster heap offset
- cluster count
- root directory cluster
- FAT count
- device pointer

## Sector I/O

`exfat_read_sectors` and `exfat_write_sectors` choose between:

- Disk callbacks, when a device pointer is supplied.
- Global SATA LBA helpers, when device is null.

This lets exFAT work either through a mounted device abstraction or through the
current SATA path.

## Cluster and FAT Operations

Important functions:

- `exfat_cluster_to_sector`: converts a cluster number to an LBA.
- `exfat_get_fat_entry`: reads one FAT entry.
- `exfat_set_fat_entry`: writes one FAT entry.
- `exfat_allocate_cluster`: finds a free cluster through the allocation bitmap,
  marks it used, and sets its FAT entry to EOC.
- `exfat_free_chain`: frees a chain and updates the allocation bitmap.

## Directory Operations

Important functions:

- `exfat_read_dir_entry`
- `exfat_write_dir_entry`
- `exfat_find_file`
- `exfat_create`
- `exfat_mkdir`
- `exfat_remove`

Filenames are handled as ASCII converted to and from UTF-16-like directory name
entries. This is enough for simple names, but not a complete Unicode
implementation.

## File Operations

Important functions:

- `exfat_open`
- `exfat_read`
- `exfat_write`
- `exfat_seek`
- `exfat_close`

`exfat_file_t` tracks:

- filesystem pointer
- first cluster
- file size
- current position
- current cluster
- cluster offset
- no-FAT-chain flag
- directory cluster and index

Writes can allocate more clusters and update file metadata.

## Formatting

`exfat_format` writes a minimal exFAT layout:

- Boot sector.
- Backup boot sector.
- FAT.
- Root directory.
- Allocation bitmap.
- Upcase-table entry.

The current format routine uses fixed choices such as 512-byte sectors,
8 sectors per cluster, FAT offset 128, and volume serial `0x12345678`.

## VFS Adapter

The VFS adapter registers exFAT as a VFS filesystem. It maps VFS operations onto
core exFAT functions and keeps a small inode cache.

Inodes are generated as:

```text
(dir_cluster << 32) | dir_index
```

Root is represented as inode `0` in the exFAT adapter.

The adapter supports:

- mount and unmount
- read and write
- create and mkdir
- listdir
- path-to-inode
- remove

## Current Limitations

- Filename conversion is ASCII-oriented.
- The inode cache is fixed at 256 entries and uses modulo replacement.
- The VFS adapter relies on cached inode metadata for many operations.
- Error handling is mostly integer return codes.
- Formatting is basic and not a complete production-grade exFAT formatter.
