/**
 @file exfat.c
  @brief Exfat driver
 @author Yazin Tantawi
 @copyright Orizon Software Foundation
 */

#include "../includes/exfat/exfat.h"
#include <string.h>
#include <stdlib.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include "../../../drivers/includes/storage/sata.h"
#include "../../../drivers/includes/storage/storage.h"
#include <log.h>
#include <serial.h>

static int exfat_set_fat_entry_raw(void* device, uint32_t fat_offset, uint32_t bytes_per_sector, uint32_t cluster, uint32_t value);
static int exfat_update_checksum(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, uint32_t dir_index);

int exfat_read_sectors(void* device, uint64_t sector, uint32_t count, void* buffer) {
    if (!device) return sata_read_lba(sector, count, buffer);
    return disk_read_callback(device, sector, count, buffer);
}

int exfat_write_sectors(void* device, uint64_t sector, uint32_t count, const void* buffer) {
    if (!device) return sata_write_lba(sector, count, buffer);
    return disk_write_callback(device, sector, count, buffer);
}

static int exfat_get_bitmap_info(exfat_fs_t* fs, uint32_t* first_cluster, uint64_t* size) {
    uint32_t index = 0; exfat_dentry_t entry;
    while (index < 128) {
        if (exfat_read_dir_entry(fs, fs->root_dir_cluster, false, index, &entry) != 0) break;
        if (entry.type == EXFAT_TYPE_EOD) break;
        if (entry.type == 0x81) {
            *first_cluster = *(uint32_t*)&entry.data[19]; *size = *(uint64_t*)&entry.data[23]; return 0;
        }
        index++;
    }
    return -1;
}

int exfat_allocate_cluster(exfat_fs_t* fs, uint32_t* cluster) {
    uint32_t bitmap_cluster; uint64_t bitmap_size;
    if (exfat_get_bitmap_info(fs, &bitmap_cluster, &bitmap_size) != 0) return -1;
    uint64_t scratch_ph = palloc(); if (!scratch_ph) return -1;
    uint8_t* cluster_buf = (uint8_t*)phys_to_virt(scratch_ph);
    uint32_t curr_clus = bitmap_cluster; uint32_t clus_idx = 0;
    while (curr_clus < EXFAT_FAT_EOC) {
        uint64_t sector = exfat_cluster_to_sector(fs, curr_clus);
        if (exfat_read_sectors(fs->device, sector, fs->sectors_per_cluster, cluster_buf) != 0) break;
        for (uint32_t i = 0; i < fs->bytes_per_cluster; i++) {
            if (cluster_buf[i] != 0xFF) {
                for (int bit = 0; bit < 8; bit++) {
                    if (!(cluster_buf[i] & (1 << bit))) {
                        uint32_t found = (clus_idx * fs->bytes_per_cluster + i) * 8 + bit + 2;
                        if (found - 2 >= fs->cluster_count) goto fail;
                        *cluster = found; cluster_buf[i] |= (1 << bit);
                        exfat_write_sectors(fs->device, sector + (i / fs->bytes_per_sector), 1, cluster_buf + (i & ~(fs->bytes_per_sector - 1)));
                        pfree(scratch_ph); exfat_set_fat_entry(fs, found, EXFAT_FAT_EOC); return 0;
                    }
                }
            }
        }
        curr_clus = exfat_get_fat_entry(fs, curr_clus); clus_idx++;
    }
fail:
    pfree(scratch_ph); return -1;
}

int exfat_format(void* device, uint64_t sector_count) {
    if (sector_count < 1024) return -1;
    uint64_t scratch_phys = palloc(); if (!scratch_phys) return -1;
    uint8_t* scratch = (uint8_t*)phys_to_virt(scratch_phys);
    memset(scratch, 0, 4096);
    exfat_boot_sector_t* boot = (exfat_boot_sector_t*)scratch;
    uint32_t bytes_per_sector = 512;
    uint32_t sectors_per_cluster = 8;
    boot->jump_boot[0] = 0xEB; boot->jump_boot[1] = 0x76; boot->jump_boot[2] = 0x90;
    memcpy(boot->fs_name, "EXFAT   ", 8);
    boot->partition_offset = 0; boot->volume_length = sector_count; boot->fat_offset = 128;
    uint32_t max_clusters = (uint32_t)(sector_count / sectors_per_cluster);
    uint32_t fat_sectors = (max_clusters * 4 + bytes_per_sector - 1) / bytes_per_sector;
    boot->fat_length = fat_sectors;
    boot->cluster_heap_offset = (boot->fat_offset + boot->fat_length + sectors_per_cluster - 1) & ~(sectors_per_cluster - 1);
    boot->cluster_count = (uint32_t)((sector_count - boot->cluster_heap_offset) / sectors_per_cluster);
    boot->root_dir_cluster = 2; boot->volume_serial = 0x12345678; boot->fs_revision = 0x0100;
    boot->bytes_per_sector_shift = 9; boot->sectors_per_cluster_shift = 3;
    boot->num_fats = 1; boot->drive_select = 0x80; boot->boot_signature = 0xAA55;
    uint32_t heap_off = boot->cluster_heap_offset; uint32_t clus_cnt = boot->cluster_count;
    uint32_t fat_off = boot->fat_offset; uint32_t fat_len = boot->fat_length;
    serial_printf("exfat_format: clusters=%u, heap=%u, fat=%u, fat_len=%u\n", clus_cnt, heap_off, fat_off, fat_len);
    if (exfat_write_sectors(device, 0, 1, boot) != 0) { pfree(scratch_phys); return -1; }
    memset(scratch, 0, 512);
    for (int i = 1; i <= 10; i++) exfat_write_sectors(device, i, 1, scratch);
    exfat_read_sectors(device, 0, 1, scratch); exfat_write_sectors(device, 12, 1, scratch);
    memset(scratch, 0, 4096);
    for (uint32_t i = 0; i < fat_len; i += 8) {
        uint32_t count = (fat_len - i < 8) ? (fat_len - i) : 8;
        if (i == 0) { uint32_t* fat = (uint32_t*)scratch; fat[0] = 0xFFFFFFF8; fat[1] = 0xFFFFFFFF; fat[2] = 0xFFFFFFFF; }
        exfat_write_sectors(device, fat_off + i, count, scratch);
        if (i == 0) memset(scratch, 0, 4096);
    }
    memset(scratch, 0, 4096); exfat_write_sectors(device, heap_off, sectors_per_cluster, scratch);
    exfat_dentry_t* entries = (exfat_dentry_t*)scratch;
    entries[0].type = EXFAT_TYPE_VOLUME_LABEL; entries[0].data[0] = 9;
    memcpy(&entries[0].data[1], "V\0N\0i\0X\0 \0D\0I\0S\0K\0", 18);
    entries[1].type = 0x81; *(uint32_t*)&entries[1].data[19] = 3; *(uint64_t*)&entries[1].data[23] = (clus_cnt + 7) / 8;
    entries[2].type = 0x82; *(uint32_t*)&entries[2].data[19] = 4; *(uint64_t*)&entries[2].data[23] = 5836;
    exfat_write_sectors(device, heap_off, 1, scratch);
    uint32_t bitmap_sectors = (uint32_t)((clus_cnt / 8 + 511) / 512);
    memset(scratch, 0, 4096);
    for (uint32_t i = 0; i < bitmap_sectors; i += 8) {
        uint32_t count = (bitmap_sectors - i < 8) ? (bitmap_sectors - i) : 8;
        if (i == 0) scratch[0] = 0x07;
        exfat_write_sectors(device, heap_off + 8 + i, count, scratch);
        if (i == 0) memset(scratch, 0, 4096);
    }
    exfat_set_fat_entry_raw(device, fat_off, 512, 3, 0xFFFFFFFF);
    exfat_set_fat_entry_raw(device, fat_off, 512, 4, 0xFFFFFFFF);
    pfree(scratch_phys); return 0;
}

static int exfat_set_fat_entry_raw(void* device, uint32_t fat_offset, uint32_t bytes_per_sector, uint32_t cluster, uint32_t value) {
    uint64_t fat_sector = fat_offset + (uint64_t)cluster * 4 / bytes_per_sector;
    uint32_t in_sector_offset = ((uint64_t)cluster * 4) % bytes_per_sector;
    uint64_t ph = palloc(); if (!ph) return -1;
    uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    exfat_read_sectors(device, fat_sector, 1, buf);
    *(uint32_t*)(buf + in_sector_offset) = value;
    int r = exfat_write_sectors(device, fat_sector, 1, buf);
    pfree(ph); return r;
}

uint16_t exfat_calc_checksum(void* data, uint32_t size) {
    uint16_t checksum = 0; uint8_t* bytes = (uint8_t*)data;
    for (uint32_t i = 0; i < size; i++) {
        if (i == 2 || i == 3) continue;
        checksum = ((checksum << 15) | (checksum >> 1)) + bytes[i];
    }
    return checksum;
}

static int exfat_update_checksum(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, uint32_t dir_index) {
    exfat_file_dentry_t file_ent;
    if (exfat_read_dir_entry(fs, dir_cluster, dir_contiguous, dir_index, (exfat_dentry_t*)&file_ent) != 0) return -1;
    int count = 1 + file_ent.secondary_count; uint16_t checksum = 0;
    for (int i = 0; i < count; i++) {
        exfat_dentry_t entry; if (exfat_read_dir_entry(fs, dir_cluster, dir_contiguous, dir_index + i, &entry) != 0) return -1;
        uint8_t* bytes = (uint8_t*)&entry;
        for (int j = 0; j < 32; j++) {
            if (i == 0 && (j == 2 || j == 3)) continue;
            checksum = ((checksum << 15) | (checksum >> 1)) + bytes[j];
        }
    }
    file_ent.checksum = checksum;
    return exfat_write_dir_entry(fs, dir_cluster, dir_contiguous, dir_index, (exfat_dentry_t*)&file_ent);
}

int exfat_utf16_to_ascii(uint16_t* utf16, char* ascii, int max_len) {
    int i = 0;
    while (i < max_len - 1 && utf16[i] != 0) {
        ascii[i] = (char)(utf16[i] & 0xFF);
        i++;
    }
    ascii[i] = '\0';
    return i;
}

static int exfat_ascii_to_utf16(const char* ascii, uint16_t* utf16, int max_chars) {
    int i = 0;
    while (i < max_chars && ascii[i] != '\0') {
        utf16[i] = (uint16_t)ascii[i];
        i++;
    }
    return i;
}

static int strcasecmp_simple(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 += 32;
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 += 32;
        }
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int exfat_mount(exfat_fs_t* fs, void* device) {
    uint64_t ph = palloc(); if (!ph) return -1;
    exfat_boot_sector_t* boot = (exfat_boot_sector_t*)phys_to_virt(ph);
    if (exfat_read_sectors(device, 0, 1, boot) != 0) { pfree(ph); return -1; }
    if (memcmp(boot->fs_name, "EXFAT   ", 8) != 0 || boot->boot_signature != 0xAA55) { pfree(ph); return -1; }
    fs->bytes_per_sector = 1 << boot->bytes_per_sector_shift;
    fs->sectors_per_cluster = 1 << boot->sectors_per_cluster_shift;
    fs->bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    fs->fat_offset = boot->fat_offset; fs->fat_length = boot->fat_length;
    fs->cluster_heap_offset = boot->cluster_heap_offset; fs->cluster_count = boot->cluster_count;
    fs->root_dir_cluster = boot->root_dir_cluster; fs->num_fats = boot->num_fats; fs->device = device;
    pfree(ph); return 0;
}

uint32_t exfat_get_fat_entry(exfat_fs_t* fs, uint32_t cluster) {
    uint64_t fat_sector = fs->fat_offset + (uint64_t)cluster * 4 / fs->bytes_per_sector;
    uint32_t fat_offset = ((uint64_t)cluster * 4) % fs->bytes_per_sector;
    uint64_t ph = palloc(); if (!ph) return EXFAT_FAT_EOC;
    uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    if (exfat_read_sectors(fs->device, fat_sector, 1, buf) != 0) { pfree(ph); return EXFAT_FAT_EOC; }
    uint32_t entry = *(uint32_t*)(buf + fat_offset); pfree(ph); return entry;
}

int exfat_set_fat_entry(exfat_fs_t* fs, uint32_t cluster, uint32_t value) {
    uint64_t fat_sector = fs->fat_offset + (uint64_t)cluster * 4 / fs->bytes_per_sector;
    uint32_t fat_offset = ((uint64_t)cluster * 4) % fs->bytes_per_sector;
    uint64_t ph = palloc(); if (!ph) return -1;
    uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    if (exfat_read_sectors(fs->device, fat_sector, 1, buf) != 0) { pfree(ph); return -1; }
    *(uint32_t*)(buf + fat_offset) = value;
    int r = exfat_write_sectors(fs->device, fat_sector, 1, buf); pfree(ph); return r;
}

uint64_t exfat_cluster_to_sector(exfat_fs_t* fs, uint32_t cluster) {
    if (cluster < 2) return 0;
    return (uint64_t)fs->cluster_heap_offset + (uint64_t)(cluster - 2) * fs->sectors_per_cluster;
}

int exfat_read_dir_entry(exfat_fs_t* fs, uint32_t cluster, bool contiguous, uint32_t index, exfat_dentry_t* entry) {
    uint32_t ents_per_clus = fs->bytes_per_cluster / sizeof(exfat_dentry_t);
    uint32_t curr_clus = cluster; uint32_t ent_off = index;
    while (ent_off >= ents_per_clus) {
        ent_off -= ents_per_clus;
        if (contiguous) curr_clus++;
        else { curr_clus = exfat_get_fat_entry(fs, curr_clus); if (curr_clus >= EXFAT_FAT_EOC) return -1; }
    }
    uint32_t sec_off = (ent_off * sizeof(exfat_dentry_t)) / fs->bytes_per_sector;
    uint32_t in_sec_off = (ent_off * sizeof(exfat_dentry_t)) % fs->bytes_per_sector;
    uint64_t sector = exfat_cluster_to_sector(fs, curr_clus) + sec_off;
    uint64_t ph = palloc(); if (!ph) return -1; uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    if (exfat_read_sectors(fs->device, sector, 1, buf) != 0) { pfree(ph); return -1; }
    memcpy(entry, buf + in_sec_off, sizeof(exfat_dentry_t)); pfree(ph); return 0;
}

int exfat_write_dir_entry(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, uint32_t index, const exfat_dentry_t* entry) {
    uint32_t curr_clus = dir_cluster; uint32_t ent_off = index;
    uint32_t ents_per_clus = fs->bytes_per_cluster / sizeof(exfat_dentry_t);
    while (ent_off >= ents_per_clus) {
        ent_off -= ents_per_clus;
        if (dir_contiguous) curr_clus++;
        else { curr_clus = exfat_get_fat_entry(fs, curr_clus); if (curr_clus >= EXFAT_FAT_EOC) return -1; }
    }
    uint32_t sec_off = (ent_off * sizeof(exfat_dentry_t)) / fs->bytes_per_sector;
    uint32_t in_sec_off = (ent_off * sizeof(exfat_dentry_t)) % fs->bytes_per_sector;
    uint64_t sector = exfat_cluster_to_sector(fs, curr_clus) + sec_off;
    uint64_t ph = palloc(); if (!ph) return -1; uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    if (exfat_read_sectors(fs->device, sector, 1, buf) != 0) { pfree(ph); return -1; }
    memcpy(buf + in_sec_off, entry, sizeof(exfat_dentry_t));
    int r = exfat_write_sectors(fs->device, sector, 1, buf); pfree(ph); return r;
}

int exfat_find_file(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, const char* name, 
                    exfat_file_dentry_t* file_entry, exfat_stream_dentry_t* stream_entry, uint32_t* out_index) {
    uint32_t index = 0; exfat_dentry_t entry;
    while (index < 1024) {
        if (exfat_read_dir_entry(fs, dir_cluster, dir_contiguous, index, &entry) != 0) break;
        if (entry.type == EXFAT_TYPE_EOD) break;
        if (entry.type == EXFAT_TYPE_FILE) {
            exfat_file_dentry_t* file = (exfat_file_dentry_t*)&entry; uint8_t sec_cnt = file->secondary_count;
            exfat_dentry_t stream_d; if (exfat_read_dir_entry(fs, dir_cluster, dir_contiguous, index + 1, &stream_d) != 0) break;
            if (stream_d.type != EXFAT_TYPE_STREAM) { index++; continue; }
            exfat_stream_dentry_t* stream = (exfat_stream_dentry_t*)&stream_d;
            char temp_name[256] = {0}; int char_pos = 0;
            for (uint8_t i = 0; i < sec_cnt - 1; i++) {
                exfat_dentry_t name_d; if (exfat_read_dir_entry(fs, dir_cluster, dir_contiguous, index + 2 + i, &name_d) != 0) break;
                if (name_d.type == EXFAT_TYPE_NAME) {
                    exfat_name_dentry_t* name_ent = (exfat_name_dentry_t*)&name_d;
                    uint16_t name_buf[15];
                    memcpy(name_buf, name_ent->name, sizeof(name_buf));
                    char_pos += exfat_utf16_to_ascii(name_buf, temp_name + char_pos, 256 - char_pos);
                }
            }
            if ((unsigned)char_pos >= sizeof(temp_name)) char_pos = sizeof(temp_name) - 1;
            temp_name[char_pos] = '\0';
            if (strcasecmp_simple(temp_name, name) == 0) {
                if (file_entry) memcpy(file_entry, file, sizeof(exfat_file_dentry_t));
                if (stream_entry) memcpy(stream_entry, stream, sizeof(exfat_stream_dentry_t));
                if (out_index) {
                    *out_index = index;
                }
                return 0;
            }
            index += sec_cnt + 1;
        } else index++;
    }
    return -1;
}

int exfat_open(exfat_fs_t* fs, const char* path, exfat_file_t* file) {
    const char* filename = path; if (*filename == '/') filename++;
    exfat_file_dentry_t fe; exfat_stream_dentry_t se; uint32_t di;
    if (exfat_find_file(fs, fs->root_dir_cluster, false, filename, &fe, &se, &di) != 0) return -1;
    file->fs = fs; file->first_cluster = se.first_cluster; file->file_size = se.data_length;
    file->position = 0; file->current_cluster = se.first_cluster; file->cluster_offset = 0;
    file->no_fat_chain = (se.flags & 0x02) != 0; file->dir_cluster = fs->root_dir_cluster;
    file->dir_contiguous = false; file->dir_index = di; return 0;
}

int exfat_create(exfat_fs_t* fs, uint32_t dir_cluster, const char* filename, exfat_file_t* file) {
    if (exfat_find_file(fs, dir_cluster, false, filename, NULL, NULL, NULL) == 0) return -1;
    int name_len = (int)strlen(filename);
    int name_ents = (name_len + 14) / 15;
    int tot_ents = 2 + name_ents;
    uint32_t idx = 0, found_idx = 0xFFFFFFFF;
    int free_cnt = 0;
    exfat_dentry_t entry;
    while (idx < 1024) {
        if (exfat_read_dir_entry(fs, dir_cluster, false, idx, &entry) != 0) {
            break;
        }
        if (entry.type == EXFAT_TYPE_EOD || (entry.type & 0x80) == 0) {
            if (free_cnt == 0) {
                found_idx = idx;
            }
            if (++free_cnt >= tot_ents) {
                break;
            }
        } else {
            free_cnt = 0;
        }
        idx++;
    }
    if (found_idx == 0xFFFFFFFF) return -1;
    exfat_file_dentry_t file_ent; memset(&file_ent, 0, sizeof(file_ent));
    file_ent.type = EXFAT_TYPE_FILE; file_ent.secondary_count = 1 + name_ents; file_ent.file_attributes = EXFAT_ATTR_ARCHIVE;
    exfat_write_dir_entry(fs, dir_cluster, false, found_idx, (exfat_dentry_t*)&file_ent);
    exfat_stream_dentry_t strm_ent; memset(&strm_ent, 0, sizeof(strm_ent));
    strm_ent.type = EXFAT_TYPE_STREAM; strm_ent.flags = 0x01; strm_ent.name_length = (uint8_t)name_len;
    exfat_write_dir_entry(fs, dir_cluster, false, found_idx + 1, (exfat_dentry_t*)&strm_ent);
    for (int i = 0; i < name_ents; i++) {
        exfat_name_dentry_t n_ent;
        memset(&n_ent, 0, sizeof(n_ent));
        n_ent.type = EXFAT_TYPE_NAME;
        uint16_t name_buf[15] = {0};
        int len = exfat_ascii_to_utf16(filename + i * 15, name_buf, 15);
        memcpy(n_ent.name, name_buf, len * sizeof(uint16_t));
        exfat_write_dir_entry(fs, dir_cluster, false, found_idx + 2 + i, (exfat_dentry_t*)&n_ent);
    }
    exfat_update_checksum(fs, dir_cluster, false, found_idx);
    file->fs = fs; file->first_cluster = 0; file->file_size = 0; file->position = 0;
    file->current_cluster = 0; file->cluster_offset = 0; file->no_fat_chain = false;
    file->dir_cluster = dir_cluster; file->dir_contiguous = false; file->dir_index = found_idx; return 0;
}

int exfat_mkdir(exfat_fs_t* fs, uint32_t dir_cluster, const char* name, uint32_t* out_cluster) {
    if (exfat_find_file(fs, dir_cluster, false, name, NULL, NULL, NULL) == 0) return -1;
    uint32_t nc;
    if (exfat_allocate_cluster(fs, &nc) != 0) {
        return -1;
    }
    uint64_t ph = palloc();
    if (!ph) {
        return -1;
    }
    uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    memset(buf, 0, fs->bytes_per_cluster);
    exfat_write_sectors(fs->device, exfat_cluster_to_sector(fs, nc), fs->sectors_per_cluster, buf);
    pfree(ph);
    int name_len = (int)strlen(name);
    int name_ents = (name_len + 14) / 15;
    int tot_ents = 2 + name_ents;
    uint32_t idx = 0, found_idx = 0xFFFFFFFF;
    int free_cnt = 0;
    exfat_dentry_t entry;
    while (idx < 1024) {
        if (exfat_read_dir_entry(fs, dir_cluster, false, idx, &entry) != 0) {
            break;
        }
        if (entry.type == EXFAT_TYPE_EOD || (entry.type & 0x80) == 0) {
            if (free_cnt == 0) {
                found_idx = idx;
            }
            if (++free_cnt >= tot_ents) {
                break;
            }
        } else {
            free_cnt = 0;
        }
        idx++;
    }
    if (found_idx == 0xFFFFFFFF) return -1;
    exfat_file_dentry_t file_ent; memset(&file_ent, 0, sizeof(file_ent));
    file_ent.type = EXFAT_TYPE_FILE; file_ent.secondary_count = 1 + name_ents; file_ent.file_attributes = EXFAT_ATTR_DIRECTORY;
    exfat_write_dir_entry(fs, dir_cluster, false, found_idx, (exfat_dentry_t*)&file_ent);
    exfat_stream_dentry_t strm_ent; memset(&strm_ent, 0, sizeof(strm_ent));
    strm_ent.type = EXFAT_TYPE_STREAM; strm_ent.flags = 0x01 | 0x02; strm_ent.name_length = (uint8_t)name_len;
    strm_ent.first_cluster = nc; strm_ent.data_length = fs->bytes_per_cluster; strm_ent.valid_data_length = fs->bytes_per_cluster;
    exfat_write_dir_entry(fs, dir_cluster, false, found_idx + 1, (exfat_dentry_t*)&strm_ent);
    for (int i = 0; i < name_ents; i++) {
        exfat_name_dentry_t n_ent;
        memset(&n_ent, 0, sizeof(n_ent));
        n_ent.type = EXFAT_TYPE_NAME;
        uint16_t name_buf[15] = {0};
        int len = exfat_ascii_to_utf16(name + i * 15, name_buf, 15);
        memcpy(n_ent.name, name_buf, len * sizeof(uint16_t));
        exfat_write_dir_entry(fs, dir_cluster, false, found_idx + 2 + i, (exfat_dentry_t*)&n_ent);
    }
    exfat_update_checksum(fs, dir_cluster, false, found_idx);
    if (out_cluster) {
        *out_cluster = nc;
    }
    return 0;
}

int exfat_read(exfat_file_t* file, void* buffer, uint64_t size) {
    if (!file || !buffer) return -1;
    uint64_t to_read = (file->position + size > file->file_size) ? file->file_size - file->position : size;
    uint64_t done = 0; uint8_t* buf = (uint8_t*)buffer;

    int order = 0;
    while ((4096UL << order) < file->fs->bytes_per_cluster) order++;
    uint64_t clus_phys = palloc_order(order);
    if (!clus_phys) return -1;
    uint8_t* clus_buf = (uint8_t*)phys_to_virt(clus_phys);

    while (to_read > 0) {
        if (file->cluster_offset >= file->fs->bytes_per_cluster) {
            if (file->no_fat_chain) file->current_cluster++;
            else { file->current_cluster = exfat_get_fat_entry(file->fs, file->current_cluster); if (file->current_cluster >= EXFAT_FAT_EOC) break; }
            file->cluster_offset = 0;
        }
        uint64_t sec = exfat_cluster_to_sector(file->fs, file->current_cluster);
        if (exfat_read_sectors(file->fs->device, sec, file->fs->sectors_per_cluster, clus_buf) != 0) break;
        uint32_t avail = file->fs->bytes_per_cluster - file->cluster_offset;
        uint32_t copy = (to_read < avail) ? (uint32_t)to_read : avail;
        memcpy(buf + done, clus_buf + file->cluster_offset, copy);
        done += copy; to_read -= copy; file->position += copy; file->cluster_offset += copy;
    }
    pfree_order(clus_phys, order); return (int)done;
}

int exfat_write(exfat_file_t* file, const void* buffer, uint64_t size) {
    if (!file || !buffer || size == 0) return 0;
    exfat_fs_t* fs = file->fs;
    serial_printf("exfat_write: clus=%u, pos=%llu, size=%llu\n", file->first_cluster, file->position, size);

    int order = 0;
    while ((4096UL << order) < fs->bytes_per_cluster) order++;
    uint64_t clus_phys = palloc_order(order);
    if (!clus_phys) return -1;
    uint8_t* clus_buf = (uint8_t*)phys_to_virt(clus_phys);

    const uint8_t* src = (const uint8_t*)buffer; uint64_t done = 0, rem = size;
    while (rem > 0) {
        if (file->first_cluster == 0) {
            uint32_t nc; if (exfat_allocate_cluster(fs, &nc) != 0) break;
            file->first_cluster = nc; file->current_cluster = nc; file->no_fat_chain = true;
            exfat_stream_dentry_t se; exfat_read_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
            se.first_cluster = nc; se.flags = 0x01 | 0x02;
            exfat_write_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
            exfat_update_checksum(fs, file->dir_cluster, file->dir_contiguous, file->dir_index);
            serial_printf("exfat_write: allocated clus %u\n", nc);
        }
        if (file->cluster_offset >= fs->bytes_per_cluster) {
            uint32_t next;
            if (file->no_fat_chain) {
                if (exfat_allocate_cluster(fs, &next) != 0) break;
                if (next == file->current_cluster + 1) file->current_cluster = next;
                else {
                    file->no_fat_chain = false;
                    for (uint32_t c = file->first_cluster; c < file->current_cluster; c++) exfat_set_fat_entry(fs, c, c + 1);
                    exfat_set_fat_entry(fs, file->current_cluster, next); file->current_cluster = next;
                    exfat_stream_dentry_t se; exfat_read_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
                    se.flags &= ~0x02; exfat_write_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
                    exfat_update_checksum(fs, file->dir_cluster, file->dir_contiguous, file->dir_index);
                }
            } else {
                next = exfat_get_fat_entry(fs, file->current_cluster);
                if (next >= EXFAT_FAT_EOC) {
                    if (exfat_allocate_cluster(fs, &next) != 0) break;
                    exfat_set_fat_entry(fs, file->current_cluster, next);
                }
                file->current_cluster = next;
            }
            file->cluster_offset = 0;
        }
        uint32_t avail = fs->bytes_per_cluster - file->cluster_offset;
        uint32_t tw = (rem < avail) ? (uint32_t)rem : avail;
        uint64_t sec = exfat_cluster_to_sector(fs, file->current_cluster);
        if (tw < fs->bytes_per_cluster) exfat_read_sectors(fs->device, sec, fs->sectors_per_cluster, clus_buf);
        memcpy(clus_buf + file->cluster_offset, src + done, tw);
        if (exfat_write_sectors(fs->device, sec, fs->sectors_per_cluster, clus_buf) != 0) break;
        done += tw; rem -= tw; file->position += tw; file->cluster_offset += tw;
        if (file->position > file->file_size) file->file_size = file->position;
    }
    exfat_stream_dentry_t se; exfat_read_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
    se.valid_data_length = file->file_size; se.data_length = file->file_size;
    exfat_write_dir_entry(fs, file->dir_cluster, file->dir_contiguous, file->dir_index + 1, (exfat_dentry_t*)&se);
    exfat_update_checksum(fs, file->dir_cluster, file->dir_contiguous, file->dir_index);
    pfree_order(clus_phys, order); return (int)done;
}

int exfat_seek(exfat_file_t* file, uint64_t position) {
    if (!file) return -1;
    if (position > file->file_size) position = file->file_size;
    file->position = 0; file->current_cluster = file->first_cluster; file->cluster_offset = 0;
    uint64_t rem = position;
    while (rem > 0 && file->current_cluster >= 2 && file->current_cluster < EXFAT_FAT_EOC) {
        if (rem < (uint64_t)file->fs->bytes_per_cluster - file->cluster_offset) {
            file->cluster_offset += (uint32_t)rem; file->position += rem; break;
        } else {
            uint64_t skip = (uint64_t)file->fs->bytes_per_cluster - file->cluster_offset;
            rem -= skip; file->position += skip;
            if (file->no_fat_chain) file->current_cluster++;
            else { file->current_cluster = exfat_get_fat_entry(file->fs, file->current_cluster); if (file->current_cluster >= EXFAT_FAT_EOC) break; }
            file->cluster_offset = 0;
        }
    }
    return 0;
}

void exfat_close(exfat_file_t* file) { (void)file; }

int exfat_update_bitmap(exfat_fs_t* fs, uint32_t cluster, bool allocated) {
    uint32_t bitmap_cluster; uint64_t bitmap_size;
    if (exfat_get_bitmap_info(fs, &bitmap_cluster, &bitmap_size) != 0) return -1;
    
    uint32_t cluster_idx = cluster - 2;
    uint32_t byte_idx = cluster_idx / 8;
    uint32_t bit_idx = cluster_idx % 8;
    
    uint32_t sector_offset = byte_idx / fs->bytes_per_sector;
    uint32_t in_sector_byte_off = byte_idx % fs->bytes_per_sector;
    
    uint64_t sector = exfat_cluster_to_sector(fs, bitmap_cluster) + sector_offset;
    
    uint64_t ph = palloc(); if (!ph) return -1;
    uint8_t* buf = (uint8_t*)phys_to_virt(ph);
    
    if (exfat_read_sectors(fs->device, sector, 1, buf) != 0) { pfree(ph); return -1; }
    
    if (allocated) buf[in_sector_byte_off] |= (1 << bit_idx);
    else buf[in_sector_byte_off] &= ~(1 << bit_idx);
    
    int r = exfat_write_sectors(fs->device, sector, 1, buf);
    pfree(ph); return r;
}

int exfat_free_chain(exfat_fs_t* fs, uint32_t first_cluster, bool contiguous, uint64_t size) {
    if (first_cluster < 2) return 0;
    
    uint32_t curr = first_cluster;
    uint32_t count = (uint32_t)((size + fs->bytes_per_cluster - 1) / fs->bytes_per_cluster);
    
    for (uint32_t i = 0; i < count || (size == 0 && i == 0); i++) {
        uint32_t next = 0;
        if (!contiguous) next = exfat_get_fat_entry(fs, curr);
        else next = curr + 1;
        
        exfat_set_fat_entry(fs, curr, EXFAT_FAT_FREE);
        exfat_update_bitmap(fs, curr, false);
        
        if (contiguous) {
            if (i + 1 >= (count == 0 ? 1 : count)) break;
            curr = next;
        } else {
            if (next >= EXFAT_FAT_EOC) break;
            curr = next;
        }
    }
    return 0;
}

int exfat_remove(exfat_fs_t* fs, uint32_t dir_cluster, const char* name) {
    exfat_file_dentry_t fe; exfat_stream_dentry_t se; uint32_t di;
    if (exfat_find_file(fs, dir_cluster, false, name, &fe, &se, &di) != 0) return -1;
    
    if (se.first_cluster >= 2) {
        exfat_free_chain(fs, se.first_cluster, (se.flags & 0x02) != 0, se.data_length);
    }
    
    int tot_ents = 1 + fe.secondary_count;
    for (int i = 0; i < tot_ents; i++) {
        exfat_dentry_t ent;
        if (exfat_read_dir_entry(fs, dir_cluster, false, di + i, &ent) == 0) {
            ent.type &= 0x7F; 
            exfat_write_dir_entry(fs, dir_cluster, false, di + i, &ent);
        }
    }
    
    return 0;
}
