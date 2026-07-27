#include "../pci/includes/pci.h"
#include "../hci/includes/hci.h"
#include "includes/ata.h" 
#include "includes/atapi.h" 
#include "includes/sata.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <kprintf.h>

legacy_storage_device_t storage_devices[MAX_STORAGE_DEVICES];

uint32_t storage_device_count = 0;

static void register_device(legacy_storage_device_t dev) {
    if (storage_device_count >= MAX_STORAGE_DEVICES) {
        log(Warn, "Max storage devices reached!\n");
        return;
    }
    storage_devices[storage_device_count++] = dev;
    log(Info, "");
    kprintf("Registered device type '%c', sector_size=%u\n", dev.type_identifier, (uint32_t)dev.sector_size);
}

void storage_init(void) {
    for (uint8_t drive = 0; drive < 2; drive++) {
        uint16_t identify_data[256];
        if (identify_ata(drive, identify_data) == 0) {
            legacy_storage_device_t dev = {0};
            dev.type_identifier = 'P';
            dev.sector_size = 512;
            dev.device_data = malloc(sizeof(uint16_t) * 256);
            memcpy(dev.device_data, identify_data, sizeof(uint16_t) * 256);
            register_device(dev);
        }
    }

    for (uint8_t drive = 0; drive < 2; drive++) {
        read10_capabillity_buffer_t buffer;
        if (identify_atapi_useful(&buffer, drive) == 0) {
            legacy_storage_device_t dev = {0};
            dev.type_identifier = 'Q';
            dev.sector_size = buffer.sector_size;
            dev.device_data = malloc(sizeof(buffer));
            memcpy(dev.device_data, &buffer, sizeof(buffer));
            register_device(dev);
        }
    }

    if (sata_get_port() != NULL) {
        legacy_storage_device_t dev = {0};
        dev.type_identifier = 'S';
        dev.sector_size = 512;
        dev.device_data = NULL;
        register_device(dev);
    }
    log(Info, "Storage initialization complete, %u device(s) found.\n",
        storage_device_count);
    hci_init();
}

