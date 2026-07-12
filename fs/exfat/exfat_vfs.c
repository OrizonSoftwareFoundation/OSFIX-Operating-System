/*
    Copyright (C) 2026 Orizon Software Foundation

    Module: exfat_vfs.c
    Description: exFAT VFS module for the OSFIX Operating System
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
#include "../../../kernel/includes/main/fsm/vfs.h"
#include "../includes/exfat/exfat.h"
#include "../../../kernel/includes/main/fsm/filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <log.h>
#include <serial.h>
#include "../../../drivers/includes/storage/sata.h"
#include "../../../kernel/includes/main/module/module.h"
#define MAX_INODE_CACHE 256

typedef struct {
    uint64_t inode;
    uint32_t first_cluster;
    uint64_t size;
    bool no_fat_chain;
    uint32_t dir_cluster;
    bool dir_contiguous;
    uint32_t dir_index;
    bool valid;
} exfat_inode_info_t;

typedef struct {
    exfat_fs_t fs;
    void* device;
    exfat_inode_info_t cache[MAX_INODE_CACHE];
} exfat_vfs_ctx_t;

static uint64_t make_inode(uint32_t dir_cluster, uint32_t dir_index) {
    return ((uint64_t)dir_cluster << 32) | dir_index;
}

static void cache_inode(exfat_vfs_ctx_t* ctx, uint64_t inode, uint32_t first_cluster, uint64_t size, bool no_fat_chain, uint32_t dir_cluster, bool dir_contiguous, uint32_t dir_index) {
    int slot = (uint32_t)(inode % MAX_INODE_CACHE);
    ctx->cache[slot].inode = inode;
    ctx->cache[slot].first_cluster = first_cluster;
    ctx->cache[slot].size = size;
    ctx->cache[slot].no_fat_chain = no_fat_chain;
    ctx->cache[slot].dir_cluster = dir_cluster;
    ctx->cache[slot].dir_contiguous = dir_contiguous;
    ctx->cache[slot].dir_index = dir_index;
    ctx->cache[slot].valid = true;
}

static bool get_cached_inode(exfat_vfs_ctx_t* ctx, uint64_t inode, exfat_inode_info_t* out) {
    int slot = (uint32_t)(inode % MAX_INODE_CACHE);
    if (ctx->cache[slot].valid && ctx->cache[slot].inode == inode) {
        memcpy(out, &ctx->cache[slot], sizeof(exfat_inode_info_t));
        return true;
    }
    return false;
}

static void* exfat_vfs_mount(void* device, uint64_t size) {
    (void)size;
    if (!device && !sata_get_port()) return NULL;
    exfat_vfs_ctx_t* ctx = malloc(sizeof(exfat_vfs_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(exfat_vfs_ctx_t));
    ctx->device = device;
    if (exfat_mount(&ctx->fs, device) != 0) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

static int exfat_vfs_unmount(void* fs_ctx) {
    if (!fs_ctx) return -1;
    free(fs_ctx);
    return 0;
}

static void prepare_file_handle(exfat_vfs_ctx_t* ctx, uint64_t inode, exfat_file_t* file) {
    memset(file, 0, sizeof(exfat_file_t));
    file->fs = &ctx->fs;
    exfat_inode_info_t info;
    if (get_cached_inode(ctx, inode, &info)) {
        file->first_cluster = info.first_cluster;
        file->current_cluster = info.first_cluster;
        file->file_size = info.size;
        file->no_fat_chain = info.no_fat_chain;
        file->dir_cluster = info.dir_cluster;
        file->dir_contiguous = info.dir_contiguous;
        file->dir_index = info.dir_index;
    } else if (inode == 0) {
        file->first_cluster = ctx->fs.root_dir_cluster;
        file->current_cluster = ctx->fs.root_dir_cluster;
        file->file_size = 0xFFFFFFFF;
        file->no_fat_chain = false;
    } else {
        uint32_t dir_c = (uint32_t)(inode >> 32);
        uint32_t dir_idx = (uint32_t)inode;
        exfat_file_dentry_t fe; exfat_stream_dentry_t se;
        if (exfat_read_dir_entry(&ctx->fs, dir_c, false, dir_idx, (exfat_dentry_t*)&fe) == 0 &&
            exfat_read_dir_entry(&ctx->fs, dir_c, false, dir_idx + 1, (exfat_dentry_t*)&se) == 0) {
            file->first_cluster = se.first_cluster; file->current_cluster = se.first_cluster;
            file->file_size = se.data_length; file->no_fat_chain = (se.flags & 0x02) != 0;
            file->dir_cluster = dir_c; file->dir_contiguous = false; file->dir_index = dir_idx;
            cache_inode(ctx, inode, se.first_cluster, se.data_length, file->no_fat_chain, dir_c, false, dir_idx);
        }
    }
}

static int exfat_vfs_read(void* fs_ctx, uint64_t inode, unsigned char* buffer, size_t size, uint64_t offset) {
    if (!fs_ctx) return -1;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    exfat_file_t file; prepare_file_handle(ctx, inode, &file);
    if (exfat_seek(&file, offset) != 0) return -1;
    return exfat_read(&file, buffer, size);
}

static int exfat_vfs_write(void* fs_ctx, uint64_t inode, unsigned char* buffer, size_t size, uint64_t offset) {
    if (!fs_ctx) return -1;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    exfat_file_t file; prepare_file_handle(ctx, inode, &file);
    if (exfat_seek(&file, offset) != 0) return -1;
    int written = exfat_write(&file, buffer, size);
    if (written > 0) cache_inode(ctx, inode, file.first_cluster, file.file_size, file.no_fat_chain, file.dir_cluster, file.dir_contiguous, file.dir_index);
    return written;
}

static uint32_t exfat_vfs_resolve_parent(exfat_vfs_ctx_t* ctx, const char* path, char* last_component) {
    uint32_t curr_dir = ctx->fs.root_dir_cluster;
    char path_copy[256]; strncpy(path_copy, path, 255); path_copy[255] = '\0';
    char* p = path_copy; if (*p == '/') p++;
    
    char* last_slash = strrchr(p, '/');
    if (last_slash) {
        *last_slash = '\0';
        char* component = p;
        while (component) {
            char* next_slash = strchr(component, '/');
            if (next_slash) *next_slash = '\0';
            
            exfat_file_dentry_t fe; exfat_stream_dentry_t se;
            if (exfat_find_file(&ctx->fs, curr_dir, false, component, &fe, &se, NULL) != 0) return 0;
            if (!(fe.file_attributes & EXFAT_ATTR_DIRECTORY)) return 0;
            curr_dir = se.first_cluster;
            
            if (next_slash) component = next_slash + 1;
            else component = NULL;
        }
        p = last_slash + 1;
    }
    
    if (last_component) strcpy(last_component, p);
    return curr_dir;
}

static uint64_t exfat_vfs_create(void* fs_ctx, const char* path) {
    if (!fs_ctx) return 0;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    char name[256]; uint32_t dir_c = exfat_vfs_resolve_parent(ctx, path, name);
    if (dir_c == 0) return 0;
    exfat_file_t file; if (exfat_create(&ctx->fs, dir_c, name, &file) != 0) return 0;
    uint64_t inode = make_inode(file.dir_cluster, file.dir_index);
    cache_inode(ctx, inode, file.first_cluster, file.file_size, file.no_fat_chain, file.dir_cluster, file.dir_contiguous, file.dir_index);
    return inode;
}

static uint64_t exfat_vfs_mkdir(void* fs_ctx, const char* path) {
    if (!fs_ctx) return 0;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    char name[256]; uint32_t dir_c = exfat_vfs_resolve_parent(ctx, path, name);
    if (dir_c == 0) return 0;
    uint32_t nc; if (exfat_mkdir(&ctx->fs, dir_c, name, &nc) != 0) return 0;
    exfat_file_dentry_t fe; exfat_stream_dentry_t se; uint32_t di;
    exfat_find_file(&ctx->fs, dir_c, false, name, &fe, &se, &di);
    uint64_t inode = make_inode(dir_c, di);
    cache_inode(ctx, inode, nc, ctx->fs.bytes_per_cluster, true, dir_c, false, di);
    return inode;
}

static int exfat_vfs_listdir(void* fs_ctx, uint64_t inode, void* entries, int max_entries) {
    if (!fs_ctx) return -1;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    uint32_t dir_cluster; bool dir_contiguous = false;
    if (inode == 0) dir_cluster = ctx->fs.root_dir_cluster;
    else {
        exfat_inode_info_t info;
        if (get_cached_inode(ctx, inode, &info)) { dir_cluster = info.first_cluster; dir_contiguous = info.no_fat_chain; }
        else { dir_cluster = (uint32_t)(inode >> 32); }
    }
    int count = 0; uint32_t index = 0; exfat_dentry_t entry;
    while (count < max_entries) {
        if (exfat_read_dir_entry(&ctx->fs, dir_cluster, dir_contiguous, index, &entry) != 0) break;
        if (entry.type == EXFAT_TYPE_EOD) break;
        if (entry.type == EXFAT_TYPE_FILE) {
            exfat_file_dentry_t* file = (exfat_file_dentry_t*)&entry;
            exfat_dentry_t stream_d; if (exfat_read_dir_entry(&ctx->fs, dir_cluster, dir_contiguous, index + 1, &stream_d) != 0) break;
            if (stream_d.type != EXFAT_TYPE_STREAM) { index++; continue; }
            exfat_stream_dentry_t* stream = (exfat_stream_dentry_t*)&stream_d;
            vfs_dirent_t* dirent = (vfs_dirent_t*)entries + count;
            dirent->inode = make_inode(dir_cluster, index);
            dirent->type = (file->file_attributes & EXFAT_ATTR_DIRECTORY) ? VFS_TYPE_DIR : VFS_TYPE_FILE;
            dirent->size = stream->data_length;
            cache_inode(ctx, dirent->inode, stream->first_cluster, stream->data_length, (stream->flags & 0x02) != 0, dir_cluster, dir_contiguous, index);
            char temp_name[256] = {0}; int char_pos = 0;
            for (uint8_t i = 0; i < file->secondary_count - 1; i++) {
                exfat_dentry_t name_d; if (exfat_read_dir_entry(&ctx->fs, dir_cluster, dir_contiguous, index + 2 + i, &name_d) != 0) break;
                if (name_d.type == EXFAT_TYPE_NAME) {
                    exfat_name_dentry_t* name_ent = (exfat_name_dentry_t*)&name_d;
                    uint16_t name_buf[15];
                    memcpy(name_buf, name_ent->name, sizeof(name_buf));
                    char_pos += exfat_utf16_to_ascii(name_buf, temp_name + char_pos, 256 - char_pos);
                }
            }
            if ((unsigned)char_pos >= sizeof(temp_name)) char_pos = sizeof(temp_name) - 1;
            temp_name[char_pos] = '\0';
            strncpy(dirent->name, temp_name, sizeof(dirent->name) - 1); dirent->name[sizeof(dirent->name) - 1] = '\0';
            count++; index += file->secondary_count + 1;
        } else index++;
    }
    return count;
}

static uint64_t exfat_vfs_path_to_inode(void* fs_ctx, const char* path) {
    if (!fs_ctx || !path) return 0;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    if (path[0] == '/' && path[1] == '\0') return 0;
    char name[256]; uint32_t dir_c = exfat_vfs_resolve_parent(ctx, path, name);
    if (dir_c == 0) return 0;
    exfat_file_dentry_t fe; exfat_stream_dentry_t se; uint32_t di;
    if (exfat_find_file(&ctx->fs, dir_c, false, name, &fe, &se, &di) != 0) return 0;
    uint64_t inode = make_inode(dir_c, di);
    cache_inode(ctx, inode, se.first_cluster, se.data_length, (se.flags & 0x02) != 0, dir_c, false, di);
    return inode;
}

static int exfat_vfs_remove(void* fs_ctx, const char* path) {
    if (!fs_ctx) return -1;
    exfat_vfs_ctx_t* ctx = (exfat_vfs_ctx_t*)fs_ctx;
    char name[256]; uint32_t dir_c = exfat_vfs_resolve_parent(ctx, path, name);
    if (dir_c == 0) return -1;
    
    uint64_t inode = exfat_vfs_path_to_inode(fs_ctx, path);
    if (inode != 0) {
        int slot = (uint32_t)(inode % MAX_INODE_CACHE);
        if (ctx->cache[slot].inode == inode) ctx->cache[slot].valid = false;
    }
    
    return exfat_remove(&ctx->fs, dir_c, name);
}

static const vfs_filesystem_t exfat_vfs_fs = {
    .fs_type = FS_TYPE_EXFAT, .name = "exFAT",
    .ops = {
        .mount = exfat_vfs_mount, .unmount = exfat_vfs_unmount, .read = exfat_vfs_read, .write = exfat_vfs_write,
        .listdir = exfat_vfs_listdir, .path_to_inode = exfat_vfs_path_to_inode, 
        .create = exfat_vfs_create, .mkdir = exfat_vfs_mkdir, .remove = exfat_vfs_remove,
    }
};

void exfat_vfs_register(void) { vfs_register_filesystem(&exfat_vfs_fs); }

int module_init(const kernel_api_t *api) {
    (void)api;
    exfat_vfs_register();
    return 0;
}