# Memory Management

OSFIX memory management is split into physical allocation, virtual mapping,
page metadata, and heap allocation.

## Physical Memory Manager

The PMM lives in `kernel/main/mm/pmm.c`. It depends on Limine's memory map and
HHDM response.

The allocator uses two layers:

- A bitmap where set bits mean allocated or reserved pages.
- Buddy-style free lists indexed by order.

Initialization does this:

1. Read the HHDM offset from Limine.
2. Read the memory map from Limine.
3. Find the highest physical address to size the bitmap.
4. Place the bitmap inside the first usable memory region large enough for it.
5. Mark every page reserved.
6. Walk usable memory regions and add aligned ranges to free lists.
7. Clear bitmap bits for free pages.

Important functions:

- `pmm_init`: initializes PMM state.
- `palloc`: allocates one 4 KiB page.
- `palloc_order`: allocates `2^order` contiguous pages.
- `pfree`: frees one page.
- `pfree_order`: frees an ordered block and tries to coalesce with buddies.
- `pmm_get_hhdm_offset`: returns the active HHDM offset.

## Virtual Memory Manager

The VMM lives in `kernel/main/mm/vmm.c`. It starts from the current CR3 provided
by the boot environment and uses the HHDM to access page tables.

Important behavior:

- `vmm_init` stores the HHDM offset and current PML4.
- `map_page` walks or allocates lower page-table levels.
- If a mapping lands inside a 2 MiB huge page, it splits that huge page into a
  normal page table before replacing the 4 KiB entry.
- `unmap_page` clears a PTE and invalidates the TLB entry.
- `virt_to_phys_user` translates a mapped virtual address back to physical.
- `phys_flush_cache` and `phys_invalidate_cache` use `clflush` and `mfence`.

User stack helpers exist:

- `create_user_stack`
- `map_user_stack_with_guard`

These map pages below `USER_STACK_TOP`, with the guard variant leaving an
unmapped guard page.

## PFN Database

The PFN database lives in `kernel/main/mm/pfndb.c`. It stores one `page_t` per
physical frame up to the maximum usable PFN.

Initialization:

1. Calculate the highest usable PFN.
2. Reserve enough usable physical memory for the database.
3. Convert the reserved physical address through HHDM.
4. Mark all pages reserved.
5. Mark usable memmap pages free.
6. Keep page zero reserved.

Useful functions:

- `pfndb_getdb`: returns the database base.
- `pfndb_getmax`: returns the highest PFN.
- `pfndb_getptr`: returns metadata for a PFN.
- `pfndb_getpfn`: converts a metadata pointer back to PFN.
- `pfndb_page_to_phys`: converts metadata to a physical address.
- `pfndb_phys_to_page`: converts a physical address to metadata.
- `pfndb_dump`: logs contiguous ranges by state.

## Heap Allocation

The heap is a TLSF pool. `kmain` allocates `palloc_order(10)`, giving 1024 pages
or 4 MiB, converts it through HHDM, and calls `tlsf_create_with_pool`.

The global `kernel_tlsf_pool` backs:

- `malloc`
- `calloc`
- `realloc`
- `free`

The wrappers live in `kernel/main/mm/heapalloc/malloc.c`.

## Current Caveats

- `malloc` depends on `kernel_tlsf_pool`; do not use it before heap setup.
- PMM and PFNDB both reserve memory from the Limine memmap, so initialization
  order matters.
- `phys_to_virt` and `virt_to_phys` assume HHDM-addressed memory.
- VMM uses the current kernel PML4 unless `load_cr3` or `set_kernel_pml4`
  changes it.
