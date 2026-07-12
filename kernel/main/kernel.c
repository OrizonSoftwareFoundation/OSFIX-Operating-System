/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include <terminal/src/flanterm_backends/fb.h>
#include <terminal/src/flanterm.h>
#include "includes/main/cpu/gdt.h"
#include "includes/main/cpu/idt.h"
#include "includes/main/cpu/isr.h"
#include "includes/main/mm/heapalloc/tlsf.h"
#include <limine.h>
#include <stddef.h>
#include <string.h>
#include <kprintf.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/pfndb.h>
#include <log.h>
#include <includes/main/fsm/initramfs/initramfs_unpacker.h>
#include <includes/main/fsm/vfs.h>
#include <includes/main/util/elf/elf_loader.h>
#include "includes/main/module/modsym.h"
#include "includes/main/module/module.h"
#include "drivers/includes/acpi/acpi.h"
#include "drivers/includes/acpi/acpi_fadt.h"
#include "drivers/includes/acpi/acpi_shtdwn.h"
#include <time/ktime.h>

volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

struct flanterm_context *global_flanterm = NULL;

volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static uint32_t get_physical_core_count(const char *vendor) {
    uint32_t eax, ebx, ecx, edx;

    if (strcmp(vendor, "GenuineIntel") == 0) {
        cpuid(4, 0, &eax, &ebx, &ecx, &edx);
        return ((eax >> 26) & 0x3F) + 1;
    }

    else if (strcmp(vendor, "AuthenticAMD") == 0) {
        cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);
        return (ecx & 0xFF) + 1;
    }

    return 1;
}

tlsf_t kernel_tlsf_pool;

#define MAX_LOADED_MODULES 10000
static const char *loaded_modules[MAX_LOADED_MODULES];
static int loaded_module_count = 0;

static int module_already_loaded(const char *path) {
    for (int i = 0; i < loaded_module_count; i++)
        if (strcmp(loaded_modules[i], path) == 0)
            return 1;
    return 0;
}

static void load_module(const char *path) {
    if (module_already_loaded(path)) return;

    module_load_info_t info;
    if (elf_load_module(path, &info) == 0) {
        LOG_OK("Loaded module: %s\n", path);
        if (loaded_module_count < MAX_LOADED_MODULES)
            loaded_modules[loaded_module_count++] = path;
        module_init_fn init = (module_init_fn)(uintptr_t)info.entry;
        init(NULL);
    } else {
        LOG_WARN("Failed to load module: %s\n", path);
    }
}

static void load_elfs_in_dir(vfs_mount_t *mnt, const char *path) {
    uint64_t inode = vfs_path_to_inode(mnt, path);
    if (!inode) return;

    static vfs_dirent_t entries[64];
    int count = vfs_listdir(mnt, inode, entries, 64);
    if (count < 0) return;

    for (int i = 0; i < count; i++) {
        static char full_path[512];
        size_t plen = strlen(path);
        if (path[plen - 1] == '/')
            snprintf(full_path, sizeof(full_path), "%s%s", path, entries[i].name);
        else
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entries[i].name);

        if (entries[i].type == VFS_TYPE_DIR) {
            load_elfs_in_dir(mnt, full_path);
        } else if (entries[i].type == VFS_TYPE_FILE) {
            size_t len = strlen(entries[i].name);
            if (len > 4 && strcmp(entries[i].name + len - 4, ".elf") == 0)
                load_module(full_path);
        }
    }
}

void kmain(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        goto halt;
    }
        extern uint64_t boot_tsc;
    boot_tsc = read_tsc_fast();
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    static uint32_t default_bg = 0x00282828;
    static uint32_t default_fg = 0x00ebdbb2;
    global_flanterm = flanterm_fb_init(
        NULL, NULL, (uint32_t *)fb->address, fb->width, fb->height, fb->pitch,
        fb->red_mask_size, fb->red_mask_shift, fb->green_mask_size, fb->green_mask_shift,
        fb->blue_mask_size, fb->blue_mask_shift, NULL, NULL, NULL, &default_bg, &default_fg,
        NULL, NULL, NULL, 0, 0, 1, 0, 0, 0, 0
    );

    init_serial();
        char vendor[15];
    uint32_t eax, ebx, ecx, edx;

    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    LOG_INFO("CPU Vendor: %s\n", vendor);

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    uint32_t logical_processors = (ebx >> 16) & 0xFF;
    LOG_INFO("Logical processors: %u\n", logical_processors);

    uint32_t physical_cores = get_physical_core_count(vendor);
    LOG_INFO("Physical cores: %u\n", physical_cores);

    GDT_Initialize();
    IDT_Initialize();
    set_CPU_clock_speed();
    ISR_Initialize();
    pmm_init();
    vmm_init();

    if (rsdp_request.response) {
        acpi_init_from_addr(rsdp_request.response->address);
    } else {
        acpi_init();
    }
    acpi_parse_fadt();

    
    uint64_t heap_phys = palloc_order(10);
    uint64_t hhdm = pmm_get_hhdm_offset();
    tlsf_t pool = tlsf_create_with_pool((void *)(heap_phys + hhdm), 4096 * 1024);
    pfndb_init(memmap_request.response);

    void *test_alloc = tlsf_malloc(pool, 64);
    if (test_alloc) {
        LOG_OK("Heap initialized and allocated 64 bytes\n");
        LOG_INFO("Heap initialized and created heap pool at %lx\n", (uint64_t)test_alloc);
        tlsf_free(pool, test_alloc);
        kernel_tlsf_pool = pool;
    } else {
        LOG_FATAL("Heap allocation failed, halting system...\n");
        SERIAL(Fatal, kmain, "Heap allocation failed, halting system...\n");
        for (;;) __asm__ volatile("hlt");
    }

    initramfs_init();

    

    static const char *priority_modules[] = {
        "/apic/apic.elf",
        "/storage/storage.elf",
        "/pci/pci.elf",
        NULL
    };

    vfs_mount_t *mnt = vfs_get_root_mount();
    for (int i = 0; priority_modules[i]; i++)
        load_module(priority_modules[i]);

    load_elfs_in_dir(mnt, "/");

    

halt:
    while (1) {}
}

//Why must god torture good people? Oh, wait, I'm not a good person. Nevermind then. -Yazin T.