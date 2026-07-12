#ifndef STORAGE_DEVICE_H
#define STORAGE_DEVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../../kernel/includes/main/fsm/filesystem.h"

typedef struct { 
    size_t port_base; 
    size_t mmio_base; 
    uint16_t flags;
} sata_charicteristics_t;

typedef struct { 
    uint8_t drive_number;
    uint16_t flags;
    uint16_t pmio_base; 
} pata_charicteristics_t;

typedef struct storage_device {
    

    char type_identifier;

    union {
        sata_charicteristics_t sata;
        pata_charicteristics_t pata;
        struct { int controller; } usb;
        struct { int pci_slot; } nvme;

    } device_data;

    uint32_t sector_size;
    uint64_t capacity;
} storage_device_t;

typedef struct storage_component {
    storage_device_t storage_device;
    filesystem_list_t *filesystem_list;
    char device_name[64];
} storage_component_t;

typedef int (*disk_read_t)(storage_device_t *storage_dev, uint64_t lba, uint32_t byte_count, void *buffer);
typedef int (*disk_write_t)(storage_device_t *storage_dev, uint64_t lba, uint32_t byte_count, const void *buffer);

int drive_read(storage_device_t *device, uint64_t lba, uint32_t sector_count, void *buf);
int drive_write(storage_device_t *device, uint64_t lba, uint32_t sector_count, const void *buf);
int list_directories(storage_device_t *device, filesystem_t *fs, disk_read_t read_disk, char *directory_path);

typedef struct disk_handle {
    void *ahci_dev; 
    uint64_t sector_count;
    uint64_t partition_offset;
    int (*read_sector)(struct disk_handle *disk, uint64_t lba, uint8_t *buffer);
    int (*write_sector)(struct disk_handle *disk, uint64_t lba, const uint8_t *buffer);
    void *dev_private;
} disk_handle_t;

int disk_read_callback(void *device, uint64_t sector, uint32_t count, void *buffer);
int disk_write_callback(void *device, uint64_t sector, uint32_t count, const void *buffer);

#endif
