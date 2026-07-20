#include <limine.h>
#include <flanterm.h>
#include "utils/terminal/flanterm_backends/fb.h"
#include <stddef.h>
#include <kprintf.h>
#include "mm/includes/heapalloc/tlsf.h"
#include <gdt.h>
#include <idt.h>
#include <vmm.h>
#include <pmm.h>
#include <isr.h>
#include <ktime.h>
#include <pfndb.h>
#include <stdint.h>
#include <vfs.h>
#include <initramfs_unpacker.h>

//After boot, this kernel image is decompressed by the bootstub, 
//loaded into memory, given the correct bootstructs in a process that goes from
//limine > decompressor bootstub > kernel, then the kernel continues on its journey
//initializing the rest of the OS. -Y.T
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_framebuffer_request framebuffer_request;

struct flanterm_context *global_flanterm = NULL;

tlsf_t kernel_tlsf_pool;


void kmain(void) {
    extern uint64_t boot_tsc;
    //without this, time wouldn't function and cause a triple fault
    boot_tsc = read_tsc_fast();
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    static uint32_t default_bg = 0x00000000;
    static uint32_t default_fg = 0xAAAAAAAA;
    global_flanterm = flanterm_fb_init(
        NULL, 
        NULL, 
        (uint32_t *)fb->address, 
        fb->width, 
        fb->height, 
        fb->pitch,
        fb->red_mask_size, 
        fb->red_mask_shift, 
        fb->green_mask_size, 
        fb->green_mask_shift,
        fb->blue_mask_size, 
        fb->blue_mask_shift, 
        NULL, 
        NULL, 
        NULL, 
        &default_bg, 
        &default_fg,
        NULL, 
        NULL, 
        NULL, 
        0, 
        0, 
        1, 
        0, 
        0, 
        0, 
        0
    );

//if this doesnt get called, the time wont be aligned correctly.
set_CPU_clock_speed();
    GDT_Initialize();
    IDT_Initialize();    
    ISR_Initialize();
    pmm_init();
    vmm_init();
    uint64_t heap_phys = palloc_order(10);
    uint64_t hhdm = pmm_get_hhdm_offset();
    tlsf_t pool = tlsf_create_with_pool((void *)(heap_phys + hhdm), 4096 * 1024);
    pfndb_init(memmap_request.response);

    void *test_alloc = tlsf_malloc(pool, 64);
    if (test_alloc) {
        log(Info, "Heap initialized and created heap pool at %lx\n", (uint64_t)test_alloc);
        tlsf_free(pool, test_alloc);
        kernel_tlsf_pool = pool;
    } else {
        log(Fatal, "Heap allocation failed, halting system...\n");
        for (;;) __asm__ volatile("hlt");
    }


    //TODO: elf loader > scheduler and SMP > initramfs loading
    //vfs_init();
   // initramfs_init();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}