OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_stub_start)

/*
  memmap for the kernel and stub (all addresses are also referenced, as literal constants,
  from kernel/linker.ld; meaning if you change one, change the other):
 
    0xffffffff80100000   stub .text/.rodata/.data/.bss + embedded
                          compressed kernel blob (budget: 2 MiB)

    0xffffffff80300000   fixed addresses of the 4 Limine request structs
                          (shared with the real kernel via `extern` +
                          PROVIDE(), budget: 4 KiB, 64 bytes apart)

    0xffffffff80400000   reserved, zero-filled region the real kernel is
                          decompressed into (budget: 16 MiB)
 
  physical (LMA) addresses mirror the virtual offsets 1:1 below 2GiB;
  same convention the previous single executable kernel/linker.ld used.
 -Yazin T. */

STUB_VMA_OFFSET = 0xffffffff80000000;
STUB_LMA_BASE   = 0x100000;

RSDP_REQ_VMA        = 0xffffffff80300000;
MEMMAP_REQ_VMA       = 0xffffffff80300040;
HHDM_REQ_VMA         = 0xffffffff80300080;
FRAMEBUFFER_REQ_VMA  = 0xffffffff803000c0;

KERNEL_TARGET_VMA  = 0xffffffff80400000;
KERNEL_TARGET_SIZE = 0x1000000; /* 16 mb headroom */

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ;
    rodata  PT_LOAD    FLAGS((1 << 2)) ;
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ;
    reqs    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ;

    /* RWX: the real kernel's .text/.rodata/.data/.bss are all flattened
       into this one blob and decompressed as one memcpy, so this single segment 
       has to cover the executable part too (bit 0 = PF_X). this gives up W^X for 
       the real kernel as a whole for now. -The big T.*/
       
    ktarget PT_LOAD    FLAGS((1 << 0) | (1 << 1) | (1 << 2)) ;
}

SECTIONS
{
    . = STUB_VMA_OFFSET + STUB_LMA_BASE;

    .text : AT(ADDR(.text) - STUB_VMA_OFFSET) {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : AT(ADDR(.rodata) - STUB_VMA_OFFSET) {
        *(.rodata .rodata.*)
        *(.compressed_kernel)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : AT(ADDR(.data) - STUB_VMA_OFFSET) {
        *(.data .data.*)
    } :data

    .bss : AT(ADDR(.bss) - STUB_VMA_OFFSET) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    ASSERT(SIZEOF(.text) + SIZEOF(.rodata) + SIZEOF(.data) + SIZEOF(.bss)
           < (RSDP_REQ_VMA - (STUB_VMA_OFFSET + STUB_LMA_BASE)),
           "stub + embedded compressed kernel blob grew past its 2MiB budget - raise RSDP_REQ_VMA in boot/stub/linker.ld")

    /* fixed address limine request structs. each gets its own output
       section pinned to an exact address so kernel/linker.ld can PROVIDE()
       the same extern symbol names at the same addresses without additional info. -Yazin T.*/

    . = RSDP_REQ_VMA;
    .rsdp_request : AT(ADDR(.rsdp_request) - STUB_VMA_OFFSET) { KEEP(*(.rsdp_request)) } :reqs

    . = MEMMAP_REQ_VMA;
    .memmap_request : AT(ADDR(.memmap_request) - STUB_VMA_OFFSET) { KEEP(*(.memmap_request)) } :reqs

    . = HHDM_REQ_VMA;
    .hhdm_request : AT(ADDR(.hhdm_request) - STUB_VMA_OFFSET) { KEEP(*(.hhdm_request)) } :reqs

    . = FRAMEBUFFER_REQ_VMA;
    .framebuffer_request : AT(ADDR(.framebuffer_request) - STUB_VMA_OFFSET) { KEEP(*(.framebuffer_request)) } :reqs

    /* reserved, zero filled target for the decompressed real kernel.
       NOLOAD => no file content; this is what lets us safely
       write to this address from the stub without a page fault happening. -Yazin T.*/
    
    . = KERNEL_TARGET_VMA;
    .kernel_target (NOLOAD) : AT(ADDR(.kernel_target) - STUB_VMA_OFFSET) {
        kernel_target_start = .;
        . += KERNEL_TARGET_SIZE;
        kernel_target_end = .;
    } :ktarget

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}

/*If kernel compression didnt exist, i'd probably have never implemented it, but its a good thing to have. -The giant T.*/