OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kmain)

/*
 * The real kernel is no longer booted directly by Limine - it is
 * compressed, embedded into the stub (boot/stub/stub.c), and decompressed
 * into memory at runtime. Its link address below MUST exactly match
 * KERNEL_TARGET_VMA in boot/stub/linker.ld, since -O binary flattens this
 * ELF by virtual address and the stub copies those bytes back starting at
 * that same address; if the two ever disagree, every absolute address the
 * compiler baked into this kernel (this is -mcmodel=kernel, non-PIE) will
 * point at the wrong place.
 */
KERNEL_VMA_OFFSET = 0xffffffff80400000;
KERNEL_LMA_BASE   = 0x400000;   /* cosmetic only - see note above, this ELF's
                                   program headers are discarded by objcopy
                                   -O binary and never used to load anything */

/*
 * These four addresses MUST match boot/stub/linker.ld's RSDP_REQ_VMA /
 * MEMMAP_REQ_VMA / HHDM_REQ_VMA / FRAMEBUFFER_REQ_VMA exactly. They give
 * kernel.c, pmm.c and pfndb.c's `extern volatile struct limine_..._request`
 * declarations the address of the actual, Limine-populated struct
 * instances, which live in the stub binary (see boot/stub/stub.c).
 */
PROVIDE(rsdp_request        = 0xffffffff80300000);
PROVIDE(memmap_request      = 0xffffffff80300040);
PROVIDE(hhdm_request        = 0xffffffff80300080);
PROVIDE(framebuffer_request = 0xffffffff803000c0);

PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ;
    rodata  PT_LOAD    FLAGS((1 << 2)) ;
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ;
}

SECTIONS
{
    . = KERNEL_VMA_OFFSET;

    .text : AT(ADDR(.text) - KERNEL_VMA_OFFSET + KERNEL_LMA_BASE) {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA_OFFSET + KERNEL_LMA_BASE) {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data : AT(ADDR(.data) - KERNEL_VMA_OFFSET + KERNEL_LMA_BASE) {
        *(.data .data.*)
    } :data

    .bss : AT(ADDR(.bss) - KERNEL_VMA_OFFSET + KERNEL_LMA_BASE) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
