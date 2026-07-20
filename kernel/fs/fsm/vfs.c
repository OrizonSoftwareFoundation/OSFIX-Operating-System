/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "includes/vfs.h"
#include <stdlib.h>
#include <string.h>
#include <serial.h>
#include <kprintf.h>
#define VFS_MAX_FILESYSTEMS 200 
#define VFS_MAX_MOUNTS      200 

static const vfs_filesystem_t *registered_fs[VFS_MAX_FILESYSTEMS];

static int registered_count = 0;

static vfs_mount_t mounts[VFS_MAX_MOUNTS];

static int mount_count = 0;

static char mount_paths[VFS_MAX_MOUNTS][64];

vfs_mount_t *vfs_get_mount_for_path(const char *path, const char **rel_path) {
    int best = -1;
    size_t best_len = 0;
    for (int i = 0; i < mount_count; i++) {
        const char *mp = mount_paths[i];
        if (!mp[0]) continue;
        size_t mplen = strlen(mp);
        if (strncmp(path, mp, mplen) == 0) {
            if (path[mplen] == '/' || path[mplen] == '\0') {
                if (mplen > best_len) { best = i; best_len = mplen; }
            }
        }
    }
    if (best < 0) {
        if (rel_path) *rel_path = path;
        return mount_count > 0 ? &mounts[0] : NULL;
    }
    if (rel_path) {
        *rel_path = path + best_len;
        if (**rel_path == '\0') *rel_path = "/";
    }
    return &mounts[best];
}

void vfs_init(void)
{
    registered_count = 0;
    mount_count = 0;
    log(Ok, "VFS successfully initialized\n");
}

int vfs_register_filesystem(const vfs_filesystem_t *fs)
{
    if (!fs) {
        log(Fatal, "VFS: cannot register NULL filesystem\n");
        return -1;
    }
    if (registered_count >= VFS_MAX_FILESYSTEMS) {
        log(Fatal,"VFS: filesystem registry full\n");
        return -1;
    }
    registered_fs[registered_count++] = fs;
    log(Info, "VFS: registered filesystem '%s' (type %u)\n", fs->name, fs->fs_type);
    return 0;
}

static const vfs_filesystem_t *vfs_find(uint8_t type)
{
    for (int i = 0; i < registered_count; i++) {
        if (registered_fs[i]->fs_type == type)
            return registered_fs[i];
    }
    return NULL;
}

vfs_mount_t *vfs_mount(uint8_t fs_type,
                       void *device,
                       uint64_t size,
                       fs_read_callback read_cb,
                       fs_write_callback write_cb)
{
    const vfs_filesystem_t *fs = vfs_find(fs_type);
    if (!fs) {
        log(Fatal, "VFS: mount failed - filesystem type %u not registered\n", fs_type);
        return NULL;
    }
    if (mount_count >= VFS_MAX_MOUNTS) {
        log(Fatal, "VFS: too many mounts\n");
        return NULL;
    }

    void *ctx = NULL;
    if (fs->ops.mount)
        ctx = fs->ops.mount(device, size);

    if (!ctx) {
        log(Fatal, "VFS: filesystem '%s' mount routine failed\n", fs->name);
        return NULL;
    }

    int idx = mount_count++;
    vfs_mount_t *mnt = &mounts[idx];
    mnt->fs = (vfs_filesystem_t *)fs;
    mnt->fs_ctx = ctx;
    mnt->device_ptr = device;
    mnt->read_cb = read_cb;
    mnt->write_cb = write_cb;
    mnt->sector_count = size;

    if      (fs_type == 0x09) strncpy(mount_paths[idx], "/proc", 63);
    else if (fs_type == 0x0A) strncpy(mount_paths[idx], "/dev",  63);
    else                      strncpy(mount_paths[idx], "/",     63);
    mount_paths[idx][63] = '\0';

    log(Ok, "VFS: Successfully mounted '%s' (type %u)\n", fs->name, fs_type);

    return mnt;
}

vfs_mount_t *vfs_get_root_mount(void)
{
    if (mount_count > 0) return &mounts[0];
    return NULL;
}

int vfs_get_mount_count(void) { return mount_count; }

const char *vfs_get_mount_path(int idx) {
    if (idx < 0 || idx >= mount_count) return NULL;
    return mount_paths[idx];
}

vfs_mount_t *vfs_get_mount_by_index(int idx) {
    if (idx < 0 || idx >= mount_count) return NULL;
    return &mounts[idx];
}

int vfs_unmount(vfs_mount_t *mount)
{
    if (!mount || !mount->fs || !mount->fs->ops.unmount)
        return -1;
    int rc = mount->fs->ops.unmount(mount->fs_ctx);
    return rc;
}

uint64_t vfs_create(vfs_mount_t *mnt, const char *path)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.create)
        return 0;
    return mnt->fs->ops.create(mnt->fs_ctx, path);
}

uint64_t vfs_mkdir(vfs_mount_t *mnt, const char *path)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.mkdir)
        return 0;
    return mnt->fs->ops.mkdir(mnt->fs_ctx, path);
}

int vfs_remove(vfs_mount_t *mnt, const char *path)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.remove)
        return -1;
    return mnt->fs->ops.remove(mnt->fs_ctx, path);
}

int vfs_rename(vfs_mount_t *mnt, const char *old_path, const char *new_path)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.rename)
        return -1;
    return mnt->fs->ops.rename(mnt->fs_ctx, old_path, new_path);
}

int vfs_stat(vfs_mount_t *mnt, uint64_t inode, vfs_stat_t *stat)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.stat)
        return -1;
    return mnt->fs->ops.stat(mnt->fs_ctx, inode, stat);
}

int vfs_chmod(vfs_mount_t *mnt, uint64_t inode, uint32_t mode)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.chmod)
        return -1;
    return mnt->fs->ops.chmod(mnt->fs_ctx, inode, mode);
}

int vfs_truncate(vfs_mount_t *mnt, uint64_t inode, uint64_t size)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.truncate)
        return -1;
    return mnt->fs->ops.truncate(mnt->fs_ctx, inode, size);
}

int vfs_read(vfs_mount_t *mnt, uint64_t inode,
             unsigned char *buf, size_t size, uint64_t offset)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.read)
        return -1;
    return mnt->fs->ops.read(mnt->fs_ctx, inode, buf, size, offset);
}

int vfs_write(vfs_mount_t *mnt, uint64_t inode,
              unsigned char *buf, size_t size, uint64_t offset)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.write)
        return -1;
    return mnt->fs->ops.write(mnt->fs_ctx, inode, buf, size, offset);
}

int vfs_listdir(vfs_mount_t *mnt, uint64_t inode,
                void *entries, int max_entries)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.listdir)
        return -1;
    return mnt->fs->ops.listdir(mnt->fs_ctx, inode, entries, max_entries);
}

uint64_t vfs_path_to_inode(vfs_mount_t *mnt, const char *path)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.path_to_inode)
        return 0;
    return mnt->fs->ops.path_to_inode(mnt->fs_ctx, path);
}

int vfs_attach(vfs_mount_t *mnt, uint64_t inode,
               unsigned char *data, size_t size)
{
    if (!mnt || !mnt->fs || !mnt->fs->ops.attach)
        return -1;
    return mnt->fs->ops.attach(mnt->fs_ctx, inode, data, size);
}
