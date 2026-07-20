/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */


#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include "filesystem.h"

typedef int (*fs_read_callback)(void *device, uint64_t sector, uint32_t count, void *buffer);

typedef int (*fs_write_callback)(void *device, uint64_t sector, uint32_t count, const void *buffer);

typedef enum {
    VFS_TYPE_FILE = 1,   

    VFS_TYPE_DIR  = 2    

} vfs_entry_type_t;

typedef struct {
    uint64_t inode;      

    uint32_t type;       

    uint64_t size;       

    char name[256];      

} vfs_dirent_t;

typedef struct {
    uint64_t inode;         

    uint64_t size;          

    uint32_t type;          

    uint32_t permissions;   

    uint32_t uid;           

    uint32_t gid;           

    uint64_t atime;         

    uint64_t mtime;         

    uint64_t ctime;         

} vfs_stat_t;

typedef struct vfs_ops {
    void *(*mount)(void *device, uint64_t size);
    int   (*unmount)(void *fs_ctx);

    int   (*read)(void *fs_ctx, uint64_t inode,
                  unsigned char *buffer, size_t size, uint64_t offset);
    int   (*write)(void *fs_ctx, uint64_t inode,
                   unsigned char *buffer, size_t size, uint64_t offset);
    int   (*attach)(void *fs_ctx, uint64_t inode, unsigned char *data, size_t size);
    int   (*listdir)(void *fs_ctx, uint64_t inode, void *entries, int max_entries);
    uint64_t (*path_to_inode)(void *fs_ctx, const char *path);
    uint64_t (*create)(void *fs_ctx, const char *path);
    uint64_t (*mkdir)(void *fs_ctx, const char *path);
    int   (*remove)(void *fs_ctx, const char *path);
    int   (*rename)(void *fs_ctx, const char *old_path, const char *new_path);
    int   (*stat)(void *fs_ctx, uint64_t inode, vfs_stat_t *stat);
    int   (*chmod)(void *fs_ctx, uint64_t inode, uint32_t mode);
    int   (*truncate)(void *fs_ctx, uint64_t inode, uint64_t size);

} vfs_ops_t;

typedef struct vfs_filesystem {
    uint8_t       fs_type;   

    const char   *name;      

    vfs_ops_t     ops;       

} vfs_filesystem_t;

typedef struct vfs_mount {
    vfs_filesystem_t *fs;         

    void              *fs_ctx;       

    void              *device_ptr;   

    fs_read_callback   read_cb;      

    fs_write_callback  write_cb;     

    uint64_t           sector_count; 

} vfs_mount_t;

int vfs_register_filesystem(const vfs_filesystem_t *fs);

vfs_mount_t *vfs_mount(uint8_t fs_type,
                       void *device,
                       uint64_t size,
                       fs_read_callback read_cb,
                       fs_write_callback write_cb);

int vfs_unmount(vfs_mount_t *mount);

vfs_mount_t *vfs_get_root_mount(void);

uint64_t vfs_create(vfs_mount_t *mnt, const char* path);

uint64_t vfs_mkdir(vfs_mount_t *mnt, const char *path);

int vfs_attach(vfs_mount_t *mnt, uint64_t inode, unsigned char *data, size_t size); 

int vfs_remove(vfs_mount_t *mnt, const char *path);

int vfs_rename(vfs_mount_t *mnt, const char *old_path, const char *new_path);

int vfs_stat(vfs_mount_t *mnt, uint64_t inode, vfs_stat_t *stat);

int vfs_chmod(vfs_mount_t *mnt, uint64_t inode, uint32_t mode);

int vfs_truncate(vfs_mount_t *mnt, uint64_t inode, uint64_t size);

int vfs_read(vfs_mount_t *mnt, uint64_t inode,
             unsigned char *buf, size_t size, uint64_t offset);

int vfs_write(vfs_mount_t *mnt, uint64_t inode,
              unsigned char *buf, size_t size, uint64_t offset);

int vfs_listdir(vfs_mount_t *mnt, uint64_t inode,
                void *entries, int max_entries);

uint64_t vfs_path_to_inode(vfs_mount_t *mnt, const char *path);

void vfs_init(void);

vfs_mount_t *vfs_get_mount_for_path(const char *path, const char **rel_path);

int          vfs_get_mount_count(void);

const char  *vfs_get_mount_path(int idx);

vfs_mount_t *vfs_get_mount_by_index(int idx);

#endif
