# Kernel API

This page is a hand-written reference for the main public interfaces exposed by
the OSFIX kernel headers. It is not generated, so update it when headers change.

## Memory: PMM

Header: `kernel/includes/main/mm/pmm.h`

- `PAGE_SIZE`: 4096 bytes.
- `MAX_ORDER`: 11, so the highest buddy block is `2^10` pages.
- `pmm_init`: initializes the physical allocator from Limine memory data.
- `palloc`: allocates one physical page and returns its physical address.
- `palloc_order`: allocates a contiguous physical block of `2^order` pages.
- `pfree`: frees one physical page.
- `pfree_order`: frees a block allocated at the same order.
- `pmm_get_total_pages`: returns total managed pages.
- `pmm_get_free_pages`: returns currently free pages.
- `pmm_get_used_pages`: returns total minus free pages.
- `pmm_get_hhdm_offset`: returns the Limine HHDM offset.

PMM functions return physical addresses. Convert them with `phys_to_virt` before
writing through them as pointers.

## Memory: VMM

Header: `kernel/includes/main/mm/vmm.h`

Common flags:

- `PTE_PRESENT`
- `PTE_WRITABLE`
- `PTE_USER`
- `PTE_WRITE_THROUGH`
- `PTE_CACHE_DISABLE`
- `PTE_GLOBAL`
- `PTE_NX`

Functions:

- `phys_to_virt`: converts a physical address into an HHDM virtual pointer.
- `virt_to_phys`: converts an HHDM virtual pointer to a physical address.
- `virt_to_phys_user`: walks page tables for a mapped virtual address.
- `vmm_init`: captures the active CR3 and HHDM offset.
- `map_page`: maps one 4 KiB page.
- `unmap_page`: clears one 4 KiB mapping.
- `phys_flush_cache`: flushes a virtual address range with `clflush`.
- `phys_invalidate_cache`: currently aliases cache flush behavior.
- `get_current_cr3`: returns the active CR3 physical address.
- `load_cr3`: switches CR3 and updates the cached kernel PML4 pointer.
- `get_kernel_pml4`: returns the cached kernel PML4.
- `set_kernel_pml4`: replaces the cached kernel PML4 pointer.
- `create_user_stack`: maps a user stack below `USER_STACK_TOP`.
- `map_user_stack_with_guard`: maps a user stack with one guard page.

## Memory: PFNDB

Header: `kernel/includes/main/mm/pfndb.h`

Page flags:

- `PAGE_FREE`
- `PAGE_USED`
- `PAGE_RESERVED`
- `PAGE_SHARED`
- `PAGE_COW`
- `PAGE_POISON`

Functions:

- `pfndb_init`: builds the page frame database from the Limine memmap.
- `pfndb_getdb`: returns the database base pointer.
- `pfndb_getmax`: returns the highest known PFN.
- `pfndb_pfnaddr`: converts a PFN to a physical address.
- `pfndb_getptr`: returns a `page_t` for a PFN.
- `pfndb_getpfn`: converts a `page_t` pointer to a PFN.
- `pfndb_phys_to_page`: converts physical address to `page_t`.
- `pfndb_page_to_phys`: converts `page_t` to physical address.
- `pfndb_dump`: logs page ranges by state.

## VFS

Header: `kernel/includes/main/fsm/vfs.h`

Types:

- `vfs_entry_type_t`: `VFS_TYPE_FILE` or `VFS_TYPE_DIR`.
- `vfs_dirent_t`: directory entry returned by list operations.
- `vfs_stat_t`: inode metadata.
- `vfs_ops_t`: filesystem operation table.
- `vfs_filesystem_t`: filesystem driver descriptor.
- `vfs_mount_t`: mounted filesystem state.

Functions:

- `vfs_register_filesystem`: registers a filesystem implementation.
- `vfs_mount`: mounts by filesystem type.
- `vfs_unmount`: unmounts through filesystem ops.
- `vfs_get_root_mount`: returns mount zero.
- `vfs_create`, `vfs_mkdir`, `vfs_remove`, `vfs_rename`.
- `vfs_stat`, `vfs_chmod`, `vfs_truncate`.
- `vfs_read`, `vfs_write`, `vfs_listdir`.
- `vfs_path_to_inode`: resolves a path inside one mount.
- `vfs_get_mount_for_path`: selects the best mount for a full path.
- `vfs_get_mount_count`, `vfs_get_mount_path`, `vfs_get_mount_by_index`.

## ELF and Modules

Headers:

- `kernel/includes/main/util/elf/elf_loader.h`
- `kernel/includes/main/module/module.h`
- `kernel/includes/main/module/ksym.h`
- `kernel/includes/main/module/modsym.h`

Executable loading:

- `elf_load`: loads an executable from VFS by path.
- `elf_load_from_memory`: loads an executable from a memory buffer.

Module loading:

- `elf_load_module`: loads a relocatable module from VFS by path.
- `module_load_info_t.image`: allocated module image.
- `module_load_info_t.image_size`: loaded image size.
- `module_load_info_t.entry`: module entry point.

Symbol lookup:

- `ksym_lookup`: searches exported kernel symbols.
- `modsym_register`: registers symbols from a loaded module.
- `modsym_dump`: logs registered module symbols.

The `kernel_api_t` structure declares function pointers for printing,
allocation, string functions, port I/O, sleep, and logging. The current loader
does not pass a populated API pointer to modules, so modules should treat it as
future-facing.

## CPU and I/O

Header: `kernel/includes/main/cpu/io.h`

Port I/O:

- `inb`, `inw`, `inl`
- `outb`, `outw`, `outl`
- `insw`, `outsw`
- `iowait`

CPU state:

- `cpuGetMSR`, `cpuSetMSR`
- `read_cr3`, `set_cr3`
- `enable_interrupts`, `disable_interrupts`
- `halt`, `halt_interrupts_enabled`, `hcf`
- `cpuid`

The header also contains a small intrusive doubly linked list helper type.

## Logging and Printing

Headers:

- `kernel/includes/main/util/print/log.h`
- `kernel/includes/main/util/print/kprintf.h`
- `kernel/includes/main/util/print/serial.h`

Logging:

- `LOG_OK`
- `LOG_INFO`
- `LOG_WARN`
- `LOG_FATAL`
- `LOG_BOTH`
- `SERIAL`

Printing:

- `kprintf`
- `sprintf`
- `snprintf`
- `vsprintf`
- `vsnprintf`
- `printcol`

Serial:

- `init_serial`
- `serial_received`
- `serial_write`
- `serial_printf`

## Time

Headers:

- `kernel/includes/main/util/time/ktime.h`
- `kernel/includes/main/util/time/tsc.h`
- `kernel/includes/main/util/time/pit.h`

Functions:

- `set_CPU_clock_speed`
- `get_time_ms`
- `get_time_us`
- `udelay`
- `measure_tsc_over_pit`
- `get_cpu_clock_speed_khz`
- `tsc_delay_us`
- `pit_read_counter`
- `pit_start_one_shot`
- `pit_delay_us`
- `pit_get_is_legit`

Timing depends on early boot setting `boot_tsc`.
