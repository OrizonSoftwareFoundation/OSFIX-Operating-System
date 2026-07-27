/*
    Copyright (C) 2026 Orizon Software Foundation

    Module: exfat.h
    Description: exFAT module for the OSFIX Operating System
    Author: Yazin Tantawi

    All components of the OSFIX Operating System, except where otherwise noted, 
    are copyright of the Orizon Software Foundation (and the corresponding author(s)) and licensed under EUPL 1.2 or later.
    For more information on the GNU Public License Version 2, please refer to the LICENSE file
    or to the link provided here: https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html

 * THIS OPERATING SYSTEM IS PROVIDED "AS IS" AND "AS AVAILABLE" UNDER 
 * THE GNU GENERAL PUBLIC LICENSE VERSION 2, WITHOUT
 * WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NON-INFRINGEMENT.
 * 
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL
 * THE AUTHORS, COPYRIGHT HOLDERS, OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE), ARISING IN ANY WAY OUT OF THE USE OF THIS OPERATING SYSTEM,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE OPERATING SYSTEM IS
 * WITH YOU. SHOULD THE OPERATING SYSTEM PROVE DEFECTIVE, YOU ASSUME THE COST OF
 * ALL NECESSARY SERVICING, REPAIR, OR CORRECTION.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF THE GNU GENERAL PUBLIC LICENSE
 * ALONG WITH THIS OPERATING SYSTEM; IF NOT, WRITE TO THE FREE SOFTWARE
 * FOUNDATION, INC., 51 FRANKLIN STREET, FIFTH FLOOR, BOSTON,
 * MA 02110-1301, USA.
*/

#ifndef EXFAT_H
#define EXFAT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];            
    uint8_t  fs_name[8];               
    uint8_t  must_be_zero[53];          
    uint64_t partition_offset;         
    uint64_t volume_length;          
    uint32_t fat_offset;              
    uint32_t fat_length;              
    uint32_t cluster_heap_offset;    
    uint32_t cluster_count;         
    uint32_t root_dir_cluster;      
    uint32_t volume_serial;        
    uint16_t fs_revision;          
    uint16_t volume_flags;          
    uint8_t  bytes_per_sector_shift;       
    uint8_t  sectors_per_cluster_shift;  
    uint8_t  num_fats;                 
    uint8_t  drive_select;            
    uint8_t  percent_in_use;          
    uint8_t  reserved[7];             
    uint8_t  boot_code[390];          
    uint16_t boot_signature;          
} exfat_boot_sector_t;

#define EXFAT_TYPE_EOD          0x00  
#define EXFAT_TYPE_ALLOCATION   0x81 
#define EXFAT_TYPE_UPCASE       0x82  
#define EXFAT_TYPE_VOLUME_LABEL 0x83 
#define EXFAT_TYPE_FILE         0x85 
#define EXFAT_TYPE_STREAM       0xC0 
#define EXFAT_TYPE_NAME         0xC1 

#define EXFAT_ATTR_READ_ONLY    0x01  
#define EXFAT_ATTR_HIDDEN       0x02  
#define EXFAT_ATTR_SYSTEM       0x04  
#define EXFAT_ATTR_DIRECTORY    0x10  
#define EXFAT_ATTR_ARCHIVE      0x20  

typedef struct __attribute__((packed)) {
    uint8_t type;        
    uint8_t data[31];     
} exfat_dentry_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  secondary_count;
    uint16_t checksum;
    uint16_t file_attributes;
    uint16_t reserved1;
    uint32_t create_timestamp;
    uint32_t modify_timestamp;
    uint32_t access_timestamp;
    uint8_t  create_10ms;
    uint8_t  modify_10ms;
    uint8_t  create_tz;
    uint8_t  modify_tz;
    uint8_t  access_tz;
    uint8_t  reserved2[7];
} exfat_file_dentry_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  flags;
    uint8_t  reserved1;
    uint8_t  name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_data_length;
    uint32_t reserved3;
    uint32_t first_cluster;
    uint64_t data_length;
} exfat_stream_dentry_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  flags;
    uint16_t name[15];
} exfat_name_dentry_t;

#define EXFAT_FAT_FREE          0x00000000
#define EXFAT_FAT_BAD           0xFFFFFFF7
#define EXFAT_FAT_EOC           0xFFFFFFFF

typedef struct {
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_dir_cluster;
    uint8_t  num_fats;
    void*    device;
} exfat_fs_t;

typedef struct {
    exfat_fs_t* fs;
    uint32_t    first_cluster;
    uint64_t    file_size;
    uint64_t    position;
    uint32_t    current_cluster;
    uint32_t    cluster_offset;
    bool        no_fat_chain;
    
    uint32_t    dir_cluster;
    bool        dir_contiguous;
    uint32_t    dir_index;
} exfat_file_t;

int exfat_mount(exfat_fs_t* fs, void* device);
int exfat_format(void* device, uint64_t sector_count);

extern int exfat_read_sectors(void* device, uint64_t sector, uint32_t count, void* buffer);
extern int exfat_write_sectors(void* device, uint64_t sector, uint32_t count, const void* buffer);

uint32_t exfat_get_fat_entry(exfat_fs_t* fs, uint32_t cluster);
uint64_t exfat_cluster_to_sector(exfat_fs_t* fs, uint32_t cluster);

int exfat_read_dir_entry(exfat_fs_t* fs, uint32_t cluster, bool contiguous, uint32_t index, exfat_dentry_t* entry);
int exfat_find_file(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, const char* name, 
                    exfat_file_dentry_t* file_entry, exfat_stream_dentry_t* stream_entry, uint32_t* out_index);

int exfat_open(exfat_fs_t* fs, const char* path, exfat_file_t* file);
int exfat_create(exfat_fs_t* fs, uint32_t dir_cluster, const char* name, exfat_file_t* file);
int exfat_mkdir(exfat_fs_t* fs, uint32_t dir_cluster, const char* name, uint32_t* out_cluster);
int exfat_remove(exfat_fs_t* fs, uint32_t dir_cluster, const char* name);
int exfat_read(exfat_file_t* file, void* buffer, uint64_t size);
int exfat_write(exfat_file_t* file, const void* buffer, uint64_t size);
int exfat_seek(exfat_file_t* file, uint64_t position);
void exfat_close(exfat_file_t* file);

int exfat_set_fat_entry(exfat_fs_t* fs, uint32_t cluster, uint32_t value);
int exfat_allocate_cluster(exfat_fs_t* fs, uint32_t* cluster);
int exfat_write_dir_entry(exfat_fs_t* fs, uint32_t dir_cluster, bool dir_contiguous, uint32_t index, const exfat_dentry_t* entry);

uint16_t exfat_calc_checksum(void* data, uint32_t size);
int exfat_utf16_to_ascii(uint16_t* utf16, char* ascii, int max_len);

#endif
