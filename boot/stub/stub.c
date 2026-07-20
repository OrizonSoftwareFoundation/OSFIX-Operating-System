//Author: Yazin T.
//License: EUPL 1.2
//Copyright (C) 2026,  Orizon Software Foundation

#include <limine.h>
#include <lz4.h>
#include <stdint.h>

//a blind man could tell what these are, these dont need comments.
__attribute__((used, section(".rsdp_request")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".memmap_request")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".hhdm_request")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".framebuffer_request")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};



//start/end of the compressed kernel that we shoved into the monstrosity we call an ELF file
extern const char _binary_build_ckImage_lz4_start[];
extern const char _binary_build_ckImage_lz4_end[];

//start of the reserved space in memory where the kernel vents all its issues in life to (decompresses to)
extern char kernel_target_start[];

#include "kernel_meta.h"   //for CKIMAGE_ENTRY and CKIMAGE_RAW_SIZE

//serial, so that our stub doesnt just give us no info when it eventually shits itself
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

//our stub shat itself, so print a message and halt.
static void stub_panic(const char *msg)
{
    while (*msg) {
        outb(0x3F8, (uint8_t)*msg);
        msg++;
    }
    for (;;)
        __asm__ volatile ("cli; hlt");
}

static void sprint(const char *msg)
{
    while (*msg) {
        outb(0x3F8, (uint8_t)*msg);
        msg++;
    }

}

void _stub_start(void)
{
    int compressed_size =
        (int)(_binary_build_ckImage_lz4_end -
              _binary_build_ckImage_lz4_start);

    int ret = LZ4_decompress_safe(
        _binary_build_ckImage_lz4_start,
        (char *)kernel_target_start,
        compressed_size,
        CKIMAGE_RAW_SIZE);

    if (ret < 0 || ret != CKIMAGE_RAW_SIZE) {
        stub_panic("stub: kernel decompression failed\r\n");
    }

    sprint("stub: Decompressing kernel...\n");

    void (*real_entry)(void) = (void (*)(void))CKIMAGE_ENTRY;
    real_entry();

    __builtin_unreachable();
}
