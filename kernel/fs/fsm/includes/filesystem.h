/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */


#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>

#define FS_TYPE_UNKNOWN    0x00 

#define FS_TYPE_FAT32      0x01 

#define FS_TYPE_EXFAT      0x02 

#define FS_TYPE_NTFS       0x03 

#define FS_TYPE_ISO9660    0x04 

#define FS_TYPE_EXT2       0x05 

#define FS_TYPE_EXT3       0x06 

#define FS_TYPE_EXT4       0x07 

#define FS_TYPE_RAMFS      0x08 

#define FS_FLAG_BOOT       0x01  

#define FS_FLAG_RECOVERY   0x02  

#define FS_FLAG_EFI        0x04  

#define FS_FLAG_INTERNAL   0x08  

#define FS_FLAG_BTRFS      0x10  

#define FS_FLAG_F2FS       0x20  

#define FS_FLAG_ZFS        0x40  

#define FS_FLAG_SWAP       0x80  

#define FS_FLAG_COMPRESSED 0x100 

#define FS_FLAG_ENCRYPTED  0x200 

typedef struct filesystem {
    char label[144];               

    char id_str[16];               

    char part_guid[16];            

    char type_guid[16];            

    uint64_t start_lba;            

    uint64_t length;               

    uint8_t fs_type;               

    void *ctx;                     

    uint32_t fs_flags;             

} filesystem_t;

typedef struct filesystem_list {
    size_t fs_count;               

    filesystem_t *fs_entries;      

} filesystem_list_t;

#endif
