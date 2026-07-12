#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <kprintf.h>
#include <string.h>
#include "kernel/includes/main/cpu/io.h"
#include "../includes/storage/sata.h"
#include "../includes/storage/scsi.h"
#include <endian.h>
#include <log.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <time/ktime.h>
#include "../includes/storage/ata.h" 
#include "../includes/storage/atapi.h" 
#include "../includes/storage/storage.h"

extern uint32_t pid_rn;

static volatile uint32_t *g_sata_port = NULL;

volatile uint32_t *sata_get_port(void) { return g_sata_port; }

int sata_read_lba(uint64_t lba, uint16_t count, void *buffer) {
    if (!g_sata_port) return -1;
    return sata_ahci_read_sector(g_sata_port, lba, count, buffer);
}

int sata_write_lba(uint64_t lba, uint16_t count, const void *buffer) {
    if (!g_sata_port) return -1;
    return sata_ahci_write_sector(g_sata_port, lba, count, buffer);
}

int sata_ahci_read_sector(volatile uint32_t *port_base, uint64_t lba, uint16_t sector_count, void *buffer) {

    uint64_t phys_addr = palloc_order(6);
    if (!phys_addr) return -1;
    ahci_port_mem_t *port_mem = (ahci_port_mem_t *)phys_to_virt(phys_addr);
    memset(port_mem, 0, 4096 * 64);

    uint64_t cl_phys = virt_to_phys(port_mem->cl);
    port_base[0x00 / 4] = (uint32_t)(cl_phys);
    port_base[0x04 / 4] = (uint32_t)(cl_phys >> 32);

    uint64_t fb_phys = virt_to_phys(port_mem->rfis);
    port_base[0x08 / 4] = (uint32_t)(fb_phys);
    port_base[0x0C / 4] = (uint32_t)(fb_phys >> 32);

    port_base[0x18 / 4] &= ~0x01;
    port_base[0x18 / 4] &= ~(1 << 4);
    while (port_base[0x18 / 4] & (1 << 15));
    while (port_base[0x18 / 4] & (1 << 14));

    hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(port_mem->cl);
    int prdt_entries = (sector_count + 15) / 16;
    cmd_header[0].flags = 5;
    cmd_header[0].prdt_length = prdt_entries;

    hba_cmd_tbl_t *cmd_tbl = (hba_cmd_tbl_t *)(port_mem->ct);
    uint64_t ct_phys = virt_to_phys(cmd_tbl);
    cmd_header[0].ctba = (uint32_t)(ct_phys);
    cmd_header[0].ctbau = (uint32_t)(ct_phys >> 32);

    uint8_t *buf = (uint8_t *)buffer;
    int remaining_sectors = sector_count;
    for (int i = 0; i < prdt_entries; i++) {
        uint64_t buf_phys = virt_to_phys(buf);
        cmd_tbl->prdt_entry[i].dba = (uint32_t)buf_phys;
        cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf_phys >> 32);
        int sectors_this_entry = (remaining_sectors < 16) ? remaining_sectors : 16;
        cmd_tbl->prdt_entry[i].dbc = (sectors_this_entry << 9) - 1;
        cmd_tbl->prdt_entry[i].i = 1;
        buf += (sectors_this_entry << 9);
        remaining_sectors -= sectors_this_entry;
    }

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmd_tbl->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x25;
    fis->device = 1 << 6;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = sector_count & 0xFF;
    fis->counth = (sector_count >> 8) & 0xFF;

    port_base[0x10 / 4] = 0xFFFFFFFF;
    port_base[0x18 / 4] |= (1 << 4);
    port_base[0x18 / 4] |= 0x01;

    phys_flush_cache(port_mem, sizeof(ahci_port_mem_t));

    port_base[0x38 / 4] = 1;

    unsigned int timeout = 1000000;
    while ((port_base[0x38 / 4] & 1) && timeout--) {
        if (port_base[0x10 / 4] & (1 << 30)) {
            kprintf("AHCI: TFES Error during read\n");
            serial_printf("AHCI: TFES Error during read\n");
            pfree_order(phys_addr, 6);
            return -1;
        }
        udelay(10);
    }

    phys_invalidate_cache(buffer, sector_count * 512);

    if (timeout == 0) {
        kprintf("AHCI: Read Timeout\n");
        serial_printf("AHCI: Read Timeout\n");
        pfree_order(phys_addr, 6);
        return -1;
    }

    phys_invalidate_cache(buffer, sector_count * 512);
    pfree_order(phys_addr, 6);
    return 0;
}

int sata_ahci_write_sector(volatile uint32_t *port_base, uint64_t lba, uint16_t sector_count, const void *buffer) {

    uint64_t phys_addr = palloc_order(6);
    if (!phys_addr) return -1;
    ahci_port_mem_t *port_mem = (ahci_port_mem_t *)phys_to_virt(phys_addr);
    memset(port_mem, 0, 4096 * 64);

    uint64_t cl_phys = virt_to_phys(port_mem->cl);
    port_base[0x00 / 4] = (uint32_t)(cl_phys);
    port_base[0x04 / 4] = (uint32_t)(cl_phys >> 32);

    uint64_t fb_phys = virt_to_phys(port_mem->rfis);
    port_base[0x08 / 4] = (uint32_t)(fb_phys);
    port_base[0x0C / 4] = (uint32_t)(fb_phys >> 32);

    port_base[0x18 / 4] &= ~0x01;
    port_base[0x18 / 4] &= ~(1 << 4);
    while (port_base[0x18 / 4] & (1 << 15));
    while (port_base[0x18 / 4] & (1 << 14));

    hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(port_mem->cl);
    int prdt_entries = (sector_count + 15) / 16;
    cmd_header[0].flags = 5 | (1 << 6);
    cmd_header[0].prdt_length = prdt_entries;

    hba_cmd_tbl_t *cmd_tbl = (hba_cmd_tbl_t *)(port_mem->ct);
    uint64_t ct_phys = virt_to_phys(cmd_tbl);
    cmd_header[0].ctba = (uint32_t)(ct_phys);
    cmd_header[0].ctbau = (uint32_t)(ct_phys >> 32);

    uint8_t *buf = (uint8_t *)buffer;
    int remaining_sectors = sector_count;
    for (int i = 0; i < prdt_entries; i++) {
        uint64_t buf_phys = virt_to_phys(buf);
        cmd_tbl->prdt_entry[i].dba = (uint32_t)buf_phys;
        cmd_tbl->prdt_entry[i].dbau = (uint32_t)(buf_phys >> 32);
        int sectors_this_entry = (remaining_sectors < 16) ? remaining_sectors : 16;
        cmd_tbl->prdt_entry[i].dbc = (sectors_this_entry << 9) - 1;
        cmd_tbl->prdt_entry[i].i = 1;
        buf += (sectors_this_entry << 9);
        remaining_sectors -= sectors_this_entry;
    }

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmd_tbl->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x35;
    fis->device = 1 << 6;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = sector_count & 0xFF;
    fis->counth = (sector_count >> 8) & 0xFF;

    port_base[0x10 / 4] = 0xFFFFFFFF;
    port_base[0x18 / 4] |= (1 << 4);
    port_base[0x18 / 4] |= 0x01;

    phys_flush_cache((void *)buffer, sector_count * 512);
    phys_flush_cache(port_mem, sizeof(ahci_port_mem_t));

    port_base[0x38 / 4] = 1;

    unsigned int timeout = 1000000;
    while ((port_base[0x38 / 4] & 1) && timeout--) {
        if (port_base[0x10 / 4] & (1 << 30)) {
            kprintf("AHCI: TFES Error during write\n");
            serial_printf("AHCI: TFES Error during write\n");
            pfree_order(phys_addr, 6);
            return -1;
        }
        udelay(10);
    }

    if (timeout == 0) {
        kprintf("AHCI: Write Timeout\n");
        serial_printf("AHCI: Write Timeout\n");
        pfree_order(phys_addr, 6);
        return -1;
    }

    pfree_order(phys_addr, 6);
    return 0;
}

int sata_ahci_identify(volatile uint32_t *port_base, void *buffer) {
    uint64_t phys_addr = palloc_order(2);
    if (!phys_addr) return -1;
    ahci_port_mem_t *port_mem = (ahci_port_mem_t *)phys_to_virt(phys_addr);
    memset(port_mem, 0, 4096 * 4);

    uint64_t cl_phys = virt_to_phys(port_mem->cl);
    port_base[0x00 / 4] = (uint32_t)(cl_phys);
    port_base[0x04 / 4] = (uint32_t)(cl_phys >> 32);

    uint64_t fb_phys = virt_to_phys(port_mem->rfis);
    port_base[0x08 / 4] = (uint32_t)(fb_phys);
    port_base[0x0C / 4] = (uint32_t)(fb_phys >> 32);

    port_base[0x18 / 4] &= ~0x11;
    while (port_base[0x18 / 4] & (1 << 15));
    while (port_base[0x18 / 4] & (1 << 14));

    hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(port_mem->cl);
    cmd_header[0].flags = 5;
    cmd_header[0].prdt_length = 1;

    hba_cmd_tbl_t *cmd_tbl = (hba_cmd_tbl_t *)(port_mem->ct);
    uint64_t ct_phys = virt_to_phys(cmd_tbl);
    cmd_header[0].ctba = (uint32_t)(ct_phys);
    cmd_header[0].ctbau = (uint32_t)(ct_phys >> 32);

    uint64_t buf_phys = virt_to_phys(buffer);
    cmd_tbl->prdt_entry[0].dba = (uint32_t)buf_phys;
    cmd_tbl->prdt_entry[0].dbau = (uint32_t)(buf_phys >> 32);
    cmd_tbl->prdt_entry[0].dbc = 512 - 1;
    cmd_tbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmd_tbl->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xEC;

    port_base[0x10 / 4] = 0xFFFFFFFF;
    port_base[0x18 / 4] |= 0x11;

    phys_invalidate_cache(buffer, 512);
    phys_flush_cache(port_mem, sizeof(ahci_port_mem_t));

    port_base[0x38 / 4] = 1;
    unsigned int timeout = 1000000;
    while ((port_base[0x38 / 4] & 1) && timeout--) udelay(10);

    phys_invalidate_cache(buffer, 512);
    pfree_order(phys_addr, 2);
    return 0;
}

void sata_search(uintptr_t mmio_base) {
    if (mmio_base == 0) return;
    kprintf("SATA: Searching MMIO base %lx\n", (uint64_t)mmio_base);
    serial_printf("SATA: Searching MMIO base %lx\n", (uint64_t)mmio_base);
    
    uint32_t pi = *(volatile uint32_t *)(mmio_base + 0x0C);
    kprintf("SATA: Port Implement Bitmask: %x\n", pi);
    serial_printf("SATA: Port Implement Bitmask: %x\n", pi);
    
    for (int p = 0; p < 32; p++) {
        if (!(pi & (1 << p))) continue;

        volatile uint32_t *port_base = (volatile uint32_t *)(mmio_base + 0x100 + p * 0x80);
        uint32_t ssts = port_base[0x28 / 4];
        uint8_t det = ssts & 0x0F;
        kprintf("SATA: Port %d SSTS: %x (DET=%d)\n", p, ssts, det);
        serial_printf("SATA: Port %d SSTS: %x (DET=%d)\n", p, ssts, det);
        
        if (det != 3) continue;

        uint32_t sig = port_base[0x24 / 4];
        kprintf("SATA: Port %d Signature: %x\n", p, sig);
        serial_printf("SATA: Port %d Signature: %x\n", p, sig);
        
        if (sig == 0x00000101) {
            kprintf("SATA: Port %d: SATA drive detected\n", p);
            serial_printf("SATA: Port %d: SATA drive detected\n", p);
            if (!g_sata_port) {
                g_sata_port = port_base;
                kprintf("SATA: g_sata_port set to %lx\n", (uint64_t)g_sata_port);
                serial_printf("SATA: g_sata_port set to %lx\n", (uint64_t)g_sata_port);
            }
        }
    }
    
}

void sata_init(void) {}

int sata_ahci_read_sector_satapi(volatile uint32_t *port_base, uint32_t lba, uint16_t sector_count, void *buffer) {
    (void)port_base; (void)lba; (void)sector_count; (void)buffer;
    return -1;
}

int sata_ahci_identify_satapi(volatile uint32_t *port_base, void *buffer) {
    (void)port_base; (void)buffer;
    return -1;
}

int sata_ahci_identify_satapi_properly(volatile uint32_t *port_base, void *buffer) {
    (void)port_base; (void)buffer;
    return -1;
}

int drive_read(storage_device_t *device, uint64_t lba, uint32_t sector_count, void *buf) {
    (void)device; (void)lba; (void)sector_count; (void)buf;
    return -1;
}

int drive_write(storage_device_t *device, uint64_t lba, uint32_t sector_count, const void *buf) {
    (void)device; (void)lba; (void)sector_count; (void)buf;
    return -1;
}

int list_directories(storage_device_t *device, filesystem_t *fs, disk_read_t read_disk, char *directory_path) {
    (void)device; (void)fs; (void)read_disk; (void)directory_path;
    return -1;
}

int disk_read_callback(void *device, uint64_t sector, uint32_t count, void *buffer) {
    if (!device) return -1;
    disk_handle_t *disk = (disk_handle_t *)device;
    if (!disk->read_sector) return -1;

    uint8_t *buf = (uint8_t *)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (disk->read_sector(disk, disk->partition_offset + sector + i, buf) != 0)
            return -1;
        buf += 512;
    }
    return 0;
}

int disk_write_callback(void *device, uint64_t sector, uint32_t count, const void *buffer) {
    if (!device) return -1;
    disk_handle_t *disk = (disk_handle_t *)device;
    if (!disk->write_sector) return -1;

    const uint8_t *buf = (const uint8_t *)buffer;
    for (uint32_t i = 0; i < count; i++) {
        if (disk->write_sector(disk, disk->partition_offset + sector + i, buf) != 0)
            return -1;
        buf += 512;
    }
    return 0;
}