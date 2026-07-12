# Filesystems

OSFIX currently has a VFS layer, a RAMFS root used by the initramfs, and an
exFAT filesystem module.

For the deeper exFAT implementation notes, see [exFAT](exfat.md).

## VFS

The VFS lives in `kernel/main/fsm/vfs.c` and
`kernel/includes/main/fsm/vfs.h`.

It provides:

- Filesystem registration.
- Mount creation and unmount dispatch.
- Root mount lookup.
- Path-to-mount selection.
- VFS wrappers for create, mkdir, remove, rename, stat, chmod, truncate, read,
  write, listdir, and path lookup.

Filesystem implementations provide a `vfs_ops_t` table. The VFS dispatches to
that table after basic null checks.

Limits:

- Up to 200 registered filesystems.
- Up to 200 mounted filesystems.
- Mount paths are fixed 64-byte strings.

Mount path selection currently maps:

- Filesystem type `0x09` to `/proc`.
- Filesystem type `0x0A` to `/dev`.
- Everything else to `/`.

## RAMFS

RAMFS lives in `kernel/main/fsm/initramfs/ramfs.c`. It is registered by
`ramfs_init` and mounted by `initramfs_init`.

RAMFS stores files in a fixed array:

- Maximum entries: 2098.
- Names are stored in 256-byte buffers.
- The root directory has inode 1 and name `/`.
- File data is allocated and resized with `malloc` and `realloc`.

Path handling is simple. Leading `/` and `./` are stripped, and an empty path is
normalized to `.`.

Current limitation: `ramfs_listdir` only returns entries for the root directory.
Nested paths can exist as names, but directory listing is flat.

## Initramfs

The initramfs loader accepts Limine modules whose path or string contains
`initramfs`.

The archive format is `newc` CPIO. Supported entry types:

- Regular files.
- Directories.

The unpacker creates directories with `vfs_mkdir`, creates files with
`vfs_create`, and writes file contents with `vfs_write`.

## exFAT Core

The exFAT implementation lives in `fs/exfat/exfat.c` and
`fs/includes/exfat/exfat.h`.

It supports:

- Mounting an exFAT volume.
- Formatting a basic exFAT volume.
- FAT entry read and write.
- Cluster allocation.
- Cluster-to-sector translation.
- Directory entry read and write.
- File lookup.
- File create.
- Directory create.
- File remove.
- File read, write, seek, and close.
- UTF-16 filename conversion to ASCII for directory listing.

More detail: [exFAT](exfat.md).

If no device pointer is supplied, exFAT falls back to SATA LBA read and write
helpers.

## exFAT VFS Bridge

The VFS bridge lives in `fs/exfat/exfat_vfs.c`.

It registers an exFAT filesystem and maps VFS operations to exFAT operations.
It also keeps a 256-entry inode cache. Inodes are made from:

```text
(dir_cluster << 32) | dir_index
```

The bridge supports:

- Mount and unmount.
- Read and write.
- Create and mkdir.
- List directory.
- Path lookup.
- Remove.
- Stat-like inode metadata through cached entries.

## Filesystem Type IDs

`kernel/includes/main/fsm/filesystem.h` declares known filesystem IDs:

- `FS_TYPE_FAT32`: `0x01`
- `FS_TYPE_EXFAT`: `0x02`
- `FS_TYPE_NTFS`: `0x03`
- `FS_TYPE_ISO9660`: `0x04`
- `FS_TYPE_EXT2`: `0x05`
- `FS_TYPE_EXT3`: `0x06`
- `FS_TYPE_EXT4`: `0x07`
- `FS_TYPE_RAMFS`: `0x08`

It also defines flags for boot, recovery, EFI, internal, btrfs, f2fs, zfs,
swap, compressed, and encrypted filesystems.

## Related Pages

- [Kernel API](kernel-api.md)
- [exFAT](exfat.md)
- [Storage](storage.md)
- [Troubleshooting](troubleshooting.md)
