/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */


#include "../includes/main/fsm/initramfs/ramfs.h"
#include <stdlib.h>
#include <string.h>
#include <serial.h>
#include <log.h>

#define MAX_FILES 2098

typedef struct {
    char name[256];         

    unsigned char *data;    

    size_t size;            

    size_t alloc_size;      

    uint64_t inode;         

    uint32_t type;          

} ram_file_t;

typedef struct {
    ram_file_t files[MAX_FILES]; 

    int file_count;              

    uint64_t next_inode;         

} ramfs_ctx_t;

static void *ramfs_mount(void *device, uint64_t size) {
    (void)device; (void)size;
    ramfs_ctx_t *ctx = malloc(sizeof(ramfs_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(ramfs_ctx_t));
    ctx->next_inode = 1;
    

    ctx->files[0].inode = ctx->next_inode++;
    ctx->files[0].type = VFS_TYPE_DIR;
    strcpy(ctx->files[0].name, "/");
    ctx->files[0].data = NULL;
    ctx->files[0].size = 0;
    ctx->files[0].alloc_size = 0;
    ctx->file_count = 1;

    return ctx;
}

static void normalize_ramfs_path(const char *path, char *out, size_t out_size) {
    const char *p = path;
    size_t i = 0;

    while ((*p == '/') || (*p == '.' && *(p+1) == '/')) {
        if (*p == '/') p++;
        else p += 2;
    }

    while (*p && i + 1 < out_size) {
        out[i++] = *p++;
    }
    out[i] = '\0';

    if (i == 0 && out_size > 0)
        out[0] = '.';
}

static uint64_t ramfs_path_to_inode(void *fs_ctx, const char *path) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    if (strcmp(path, "/") == 0) return ctx->files[0].inode;

    char norm[256];
    normalize_ramfs_path(path, norm, sizeof(norm));

    for (int i = 0; i < ctx->file_count; i++) {
        if (strcmp(ctx->files[i].name, norm) == 0) {
            return ctx->files[i].inode;
        }
    }
    return 0;
}

static int ramfs_read(void *fs_ctx, uint64_t inode, unsigned char *buffer, size_t size, uint64_t offset) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    for (int i = 0; i < ctx->file_count; i++) {
        if (ctx->files[i].inode == inode) {
            if (ctx->files[i].type == VFS_TYPE_DIR) return -1;
            if (offset >= ctx->files[i].size) return 0;
            if (offset + size > ctx->files[i].size) size = ctx->files[i].size - offset;
            memcpy(buffer, ctx->files[i].data + offset, size);
            return (int)size;
        }
    }
    return -1;
}

static int ramfs_write(void *fs_ctx, uint64_t inode, unsigned char *buffer, size_t size, uint64_t offset) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    for (int i = 0; i < ctx->file_count; i++) {
        if (ctx->files[i].inode == inode) {
            if (ctx->files[i].type == VFS_TYPE_DIR) {
                LOG_INFO("ramfs_write: inode %llu is directory\n", inode);
                SERIAL(Info, ramfs_write, "ramfs_write: inode %llu is directory\n", inode);

                return -1;
            }

            if (size == 0) return 0;

            size_t requested_end = offset + size;
            if (requested_end > ctx->files[i].alloc_size) {
                unsigned char *new_data = realloc(ctx->files[i].data, requested_end);
                if (!new_data) return -1;
                ctx->files[i].data = new_data;
                ctx->files[i].alloc_size = requested_end;
            }

            memcpy(ctx->files[i].data + offset, buffer, size);
            if (requested_end > ctx->files[i].size)
                ctx->files[i].size = requested_end;
            return (int)size;
        }
    }
    LOG_INFO("ramfs_write: inode %llu not found\n", inode);
    return -1;
}

static uint64_t ramfs_create(void *fs_ctx, const char *path) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    char norm[256];
    normalize_ramfs_path(path, norm, sizeof(norm));

    for (int i = 0; i < ctx->file_count; i++) {
        if (strcmp(ctx->files[i].name, norm) == 0)
            return ctx->files[i].inode;
    }

    if (ctx->file_count >= MAX_FILES) return 0;

    int idx = ctx->file_count++;
    ctx->files[idx].inode = ctx->next_inode++;
    ctx->files[idx].type = VFS_TYPE_FILE;
    strncpy(ctx->files[idx].name, norm, 255);
    ctx->files[idx].name[255] = '\0';
    ctx->files[idx].data = NULL;
    ctx->files[idx].size = 0;
    ctx->files[idx].alloc_size = 0;

    return ctx->files[idx].inode;
}

static uint64_t ramfs_mkdir(void *fs_ctx, const char *path) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    if (ctx->file_count >= MAX_FILES) return 0;

    char norm[256];
    normalize_ramfs_path(path, norm, sizeof(norm));

    int idx = ctx->file_count++;
    ctx->files[idx].inode = ctx->next_inode++;
    ctx->files[idx].type = VFS_TYPE_DIR;
    strncpy(ctx->files[idx].name, norm, 255);
    ctx->files[idx].name[255] = '\0';
    ctx->files[idx].data = NULL;
    ctx->files[idx].size = 0;
    ctx->files[idx].alloc_size = 0;

    return ctx->files[idx].inode;
}

static int ramfs_stat(void *fs_ctx, uint64_t inode, vfs_stat_t *stat) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    for (int i = 0; i < ctx->file_count; i++) {
        if (ctx->files[i].inode == inode) {
            stat->inode = inode;
            stat->size = ctx->files[i].size;
            stat->type = ctx->files[i].type;
            return 0;
        }
    }
    return -1;
}

static int ramfs_listdir(void *fs_ctx, uint64_t inode, void *entries, int max_entries) {
    ramfs_ctx_t *ctx = (ramfs_ctx_t *)fs_ctx;
    vfs_dirent_t *ents = (vfs_dirent_t *)entries;
    

    int dir_idx = -1;
    for (int i = 0; i < ctx->file_count; i++) {
        if (ctx->files[i].inode == inode) {
            if (ctx->files[i].type != VFS_TYPE_DIR) return -1;
            dir_idx = i;
            break;
        }
    }
    if (dir_idx == -1) return -1;

    if (inode != ctx->files[0].inode) return 0;

    int count = 0;
    for (int i = 1; i < ctx->file_count && count < max_entries; i++) {
        ents[count].inode = ctx->files[i].inode;
        ents[count].type = ctx->files[i].type;
        ents[count].size = ctx->files[i].size;
        strncpy(ents[count].name, ctx->files[i].name, 255);
        count++;
    }
    return count;
}

static vfs_filesystem_t ramfs_fs = {
    .fs_type = FS_TYPE_RAMFS,
    .name = "ramfs",
    .ops = {
        .mount = ramfs_mount,
        .path_to_inode = ramfs_path_to_inode,
        .read = ramfs_read,
        .write = ramfs_write,
        .create = ramfs_create,
        .mkdir = ramfs_mkdir,
        .stat = ramfs_stat,
        .listdir = ramfs_listdir
    }
};

void ramfs_init(void) {
    vfs_register_filesystem(&ramfs_fs);
}
