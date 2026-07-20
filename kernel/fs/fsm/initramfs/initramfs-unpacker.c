/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin Tantawi
 @license: EUPL 1.2
 */


#include <limine.h>
#include <stdint.h>
#include <string.h>
#include "../includes/vfs.h"
#include "../includes/initramfs/ramfs.h"
#include <vmm.h>
#include <pmm.h>
#include <serial.h>
#include <kprintf.h>

#define MAX_CPIO_NAME 255

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id       = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
};

#define CPIO_MAGIC     "070701"

#define CPIO_MAGIC_LEN  6

#define CPIO_IFMT   0xF000

#define CPIO_IFREG  0x8000

#define CPIO_IFDIR  0x4000

struct cpio_newc_header {
    char magic[6];

    char ino[8];

    char mode[8];

    char uid[8];

    char gid[8];

    char nlink[8];

    char mtime[8];

    char filesize[8];

    char devmajor[8];

    char devminor[8];

    char rdevmajor[8];

    char rdevminor[8];

    char namesize[8];

    char check[8];

} __attribute__((packed));

_Static_assert(sizeof(struct cpio_newc_header) == 110, "cpio_newc_header must be exactly 110 bytes");

static uint32_t hex8_to_u32(const char *s)
{
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        v <<= 4;
        char c = s[i];
        if      (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
    }
    return v;
}

static inline const unsigned char *align4(const unsigned char *p)
{
    return (const unsigned char *)(((uintptr_t)p + 3ULL) & ~3ULL); //This one piece of shit function prevented AMD systems from resolving the initramfs. I don't even know what to say at this point. -one very pissed off Yazin Tantawi
}

static inline const unsigned char *limine_to_virt(void *ptr)
{
    uintptr_t addr = (uintptr_t)ptr;
    uint64_t hhdm_offset = pmm_get_hhdm_offset();

    return (addr < hhdm_offset)
        ? (const unsigned char *)phys_to_virt(addr)
        : (const unsigned char *)addr;
}

static int normalize_path(const char *src, char *dst, size_t dstsz) {
    while (*src == '/') src++;
    size_t i = 0;
    while (*src && i + 1 < dstsz)
        dst[i++] = *src++;
    dst[i] = '\0';

    if (*src != '\0') return -1; //truncated; original name was too long

    if (i == 0 && dstsz > 0) {
        dst[0] = '.';
        dst[1] = '\0';
    }
    return 0;
}

static void unpack_cpio(vfs_mount_t *mnt,
                        const unsigned char *base,
                        size_t archive_size)
{

    const unsigned char *end = base + archive_size;
    const unsigned char *ptr = base;

    while (ptr + sizeof(struct cpio_newc_header) <= end) {

        const struct cpio_newc_header *h = (const struct cpio_newc_header *)ptr;

        if (memcmp(h->magic, CPIO_MAGIC, CPIO_MAGIC_LEN) != 0) {
            log(Fatal, "initramfs: bad magic: %x %x %x %x %x %x at offset %u\n",
                (unsigned)h->magic[0], (unsigned)h->magic[1], (unsigned)h->magic[2],
                (unsigned)h->magic[3], (unsigned)h->magic[4], (unsigned)h->magic[5],
                (unsigned)(ptr - base));

            log(Fatal, "%c%c%c%c%c%c at offset %zu\n",
                   h->magic[0], h->magic[1], h->magic[2],
                   h->magic[3], h->magic[4], h->magic[5],
                   (size_t)(ptr - base));
            break;
        }

        uint32_t namesize = hex8_to_u32(h->namesize);
        uint32_t filesize = hex8_to_u32(h->filesize);
        uint32_t mode     = hex8_to_u32(h->mode);

        ptr += sizeof(struct cpio_newc_header);

        // --- name validation: must happen before we ever touch `name`/`ptr+namesize` ---
        if (namesize == 0 || namesize > MAX_CPIO_NAME) {
            log(Fatal, "initramfs: invalid namesize (%u) - stopping\n", namesize);
            break;
        }

        if (ptr + namesize > end) {
            log(Fatal, "initramfs: name truncated - stopping\n");
            break;
        }

        const char *name = (const char *)ptr;

        if (memcmp(name, "TRAILER!!!", 10) == 0)
            break;

        ptr = align4(ptr + namesize);

        if (ptr + filesize > end) {
            log(Fatal, "initramfs: data for '%s' truncated - stopping\n", name);
            break;
        }

        const unsigned char *file_data = ptr;
        ptr = align4(ptr + filesize);

        char path[256];
        if (normalize_path(name, path, sizeof(path)) != 0) {
            log(Warn, "initramfs: path too long, skipping entry\n");
            continue;
        }

        if (path[0] == '.' && path[1] == '\0')
            continue;

        uint32_t ftype = mode & CPIO_IFMT;

        if (ftype == CPIO_IFREG) {
            uint64_t inode = vfs_create(mnt, path);
            if (!inode) {
                log(Warn, "initramfs: vfs_create failed for '%s'\n", path);
                continue;
            }
            if (filesize > 0)
                vfs_attach(mnt, inode, (unsigned char *)file_data, filesize); // zero-copy

        } else if (ftype == CPIO_IFDIR) {
            if (!vfs_mkdir(mnt, path)) {
                log(Fatal, "initramfs: vfs_mkdir failed for '%s'\n", path);
            }
        }
    }
}

void initramfs_init(void)
{
    ramfs_init();

    vfs_mount_t *mnt = vfs_mount(FS_TYPE_RAMFS, NULL, 0, NULL, NULL);
    if (!mnt) {
        log(Fatal, "initramfs: failed to mount ramfs\n");
        return;
    }

    if (!module_request.response || module_request.response->module_count == 0) {
        log(Info, "initramfs: no Limine modules present\n");
        return;
    }

    for (size_t i = 0; i < module_request.response->module_count; i++) {
        struct limine_file *mod = module_request.response->modules[i];

        int is_initramfs =
            (mod->path   && strstr(mod->path,   "initramfs")) ||
            (mod->string && strstr(mod->string, "initramfs"));

        if (!is_initramfs)
            continue;

        log(Info, "initramfs: unpacking '%s' (%d bytes)\n",
            mod->path, (unsigned long long)mod->size);

        log(Info,
               "initramfs: mod->address=%p limine_to_virt=%p size=%llu hhdm=%p\n",
               mod->address,
               limine_to_virt(mod->address),
               (unsigned long long)mod->size,
               (void *)pmm_get_hhdm_offset());

        const unsigned char *v = limine_to_virt(mod->address);
        serial_printf("Initramfs's first bytes: %x %x %x %x %x %x\n",
                      v[0], v[1], v[2], v[3], v[4], v[5]);
        unpack_cpio(mnt, limine_to_virt(mod->address), mod->size);
    }
}

//"Make an initramfs!" They said. "It's very easy and won't have you contemplating life!" They said. -Yazin Tantawi