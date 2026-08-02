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
#include <apic_irq.h>
#include <pci.h>
#include <hci.h>
#include <storage.h>
#include <sched.h>
#include <test_tasks.h>
#include <syscall.h>

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

#define USER_CODE_VADDR   0x0000000000400000ULL
#define USER_STACK_VADDR  0x0000000000500000ULL
#define PAGE_SIZE         4096


static const uint8_t test_fork_code[] = {
    0x48, 0xC7, 0xC0, 0x39, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0x48, 0x85, 0xC0,
    0x75, 0x23,

    0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,
    0x48, 0xBE, 0x56, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0xC2, 0x06, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0x21,

    0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,
    0x48, 0xBE, 0x5C, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0xC2, 0x07, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,
    0xCD, 0x80,

    0xF3, 0x90,
    0xEB, 0xFC,

    'c','h','i','l','d','\n',
    'p','a','r','e','n','t','\n',
};

void run_fork_test(void)
{
    void *code_phys = (void *)palloc();
    void *stack_phys = (void *)palloc();

    memcpy(code_phys, test_fork_code, sizeof(test_fork_code));
    map_page(USER_CODE_VADDR, (uint64_t)code_phys, PTE_PRESENT | PTE_USER);
    map_page(USER_STACK_VADDR, (uint64_t)stack_phys,
             PTE_PRESENT | PTE_USER | PTE_WRITABLE);

    static struct task_struct fork_task;
    static uint8_t fork_kstack[16384] __attribute__((aligned(16)));

    create_user_task(&fork_task, fork_kstack, sizeof(fork_kstack),
                     USER_CODE_VADDR,
                     USER_STACK_VADDR + PAGE_SIZE,
                     0);
    task[1] = &fork_task;
}

void kmain(void) {
    extern uint64_t boot_tsc;
    extern void syscall_entry(void); //needed for syscalls and whatnot
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

    ktprintf("OSFIX version 0.0.1-pre-amd64\n");
//if this doesnt get called, the time wont be aligned correctly.
set_CPU_clock_speed();
    GDT_Initialize();
    IDT_Initialize();    
    ISR_Initialize();
    IDT_SetGate(0x80, (void *)syscall_entry, GDT_CODE_SEGMENT, 0xEE);
    IDT_EnableGate(0x80);
    log(Ok, "IDT[0x80] flags immediately after set = %x\n", IDT_GetGateFlags(0x80));
    pmm_init();
    vmm_init();
    uint64_t heap_phys = palloc_order(10);
    uint64_t hhdm = pmm_get_hhdm_offset();
    tlsf_t pool = tlsf_create_with_pool((void *)(heap_phys + hhdm), 4096 * 1024);
    pfndb_init(memmap_request.response);

    void *test_alloc = tlsf_malloc(pool, 64);
    if (test_alloc) {
        log(Info, "Heap initialized and created heap pool at %x\n", (uint64_t)test_alloc);
        tlsf_free(pool, test_alloc);
        kernel_tlsf_pool = pool;
    } else {
        log(Fatal, "Heap allocation failed, halting system...\n");
        for (;;) __asm__ volatile("hlt");
    }
    APIC_IRQ_Initialize();
    storage_init();
    pci_init();
    vfs_init();
    scheduler_init();
    // initramfs_init(); soon..
    
    //not needed anymore, tasks are already proven to work
    //create_default_test_tasks();
    //create_priority_demo_tasks();
    //create_privileged_task_test();
    //run_fork_test(); this works too

    serial_printf("IDT[0x80] flags before jump = %x\n", IDT_GetGateFlags(0x80));
    //run_ring3_test(); the prerequisite here being SMP, elf loading, then initramfs

    while(1){
         schedule(); //move onto next task
         //print_task_info();
        __asm__ volatile ("hlt");
    }
}

//Why must god torture good people? Oh, wait, I'm not a good person. Nevermind then.