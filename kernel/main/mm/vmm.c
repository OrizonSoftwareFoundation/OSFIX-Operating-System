/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "includes/main/mm/vmm.h"
#include "includes/main/mm/pmm.h"
#include <log.h>
#include <serial.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint64_t  hhdm_offset;

static uint64_t *kernel_pml4;

void *phys_to_virt(uint64_t phys)  { return (void *)(phys + hhdm_offset); }

uint64_t virt_to_phys(void *virt)  { return (uint64_t)virt - hhdm_offset; }

static uint64_t *alloc_table(void)
{
    uint64_t phys = palloc();
    if (!phys) {
        SERIAL(Fatal, alloc_table, "VMM: page table allocation failed\n");
        LOG_FATAL("VMM: page table allocation failed\n");
        for (;;) __asm__ volatile("hlt");
    }
    memset(phys_to_virt(phys), 0, PAGE_SIZE);
    return (uint64_t *)phys_to_virt(phys);
}

static uint64_t *walk_or_alloc(uint64_t *table, uint64_t idx, uint64_t user_bit)
{
    if (!(table[idx] & PTE_PRESENT)) {
        uint64_t *child = alloc_table();
        table[idx] = (virt_to_phys(child) & ~0xFFFULL)
                   | PTE_PRESENT | PTE_WRITABLE | user_bit;
        return child;
    }
    table[idx] |= user_bit;
    return (uint64_t *)phys_to_virt(table[idx] & ~0xFFFULL);
}

void map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t user_bit = flags & PTE_USER;
    uint64_t *pdpt = walk_or_alloc(kernel_pml4, PML4_INDEX(virt), user_bit);
    uint64_t *pd   = walk_or_alloc(pdpt,        PDPT_INDEX(virt), user_bit);

    if (pd[PD_INDEX(virt)] & PTE_PS) {
        uint64_t huge_phys  = pd[PD_INDEX(virt)] & 0x000FFFFFFFE00000ULL;
        uint64_t huge_flags = (pd[PD_INDEX(virt)] & 0xFFFULL) & ~(uint64_t)PTE_PS;
        uint64_t *pt = alloc_table();
        for (int i = 0; i < 512; i++)
            pt[i] = (huge_phys + (uint64_t)i * PAGE_SIZE) | huge_flags | user_bit;
        pd[PD_INDEX(virt)] = (virt_to_phys(pt) & ~0xFFFULL)
                           | PTE_PRESENT | PTE_WRITABLE | user_bit;
        pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PTE_PRESENT;
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
        return;
    }

    uint64_t *pt = walk_or_alloc(pd, PD_INDEX(virt), user_bit);
    pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PTE_PRESENT;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void unmap_page(uint64_t virt)
{
    if (!(kernel_pml4[PML4_INDEX(virt)] & PTE_PRESENT)) return;
    uint64_t *pdpt = phys_to_virt(kernel_pml4[PML4_INDEX(virt)] & ~0xFFFULL);

    if (!(pdpt[PDPT_INDEX(virt)] & PTE_PRESENT)) return;
    uint64_t *pd = phys_to_virt(pdpt[PDPT_INDEX(virt)] & ~0xFFFULL);

    if (!(pd[PD_INDEX(virt)] & PTE_PRESENT)) return;
    uint64_t *pt = phys_to_virt(pd[PD_INDEX(virt)] & ~0xFFFULL);

    pt[PT_INDEX(virt)] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t virt_to_phys_user(uint64_t virt)
{
    if (!(kernel_pml4[PML4_INDEX(virt)] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = phys_to_virt(kernel_pml4[PML4_INDEX(virt)] & PTE_ADDR_MASK);

    if (!(pdpt[PDPT_INDEX(virt)] & PTE_PRESENT)) return 0;
    uint64_t *pd = phys_to_virt(pdpt[PDPT_INDEX(virt)] & PTE_ADDR_MASK);

    if (!(pd[PD_INDEX(virt)] & PTE_PRESENT)) return 0;
    if (pd[PD_INDEX(virt)] & PTE_PS)
        return (pd[PD_INDEX(virt)] & PTE_ADDR_MASK) + (virt & 0x1FFFFFULL);

    uint64_t *pt = phys_to_virt(pd[PD_INDEX(virt)] & PTE_ADDR_MASK);
    if (!(pt[PT_INDEX(virt)] & PTE_PRESENT)) return 0;
    return (pt[PT_INDEX(virt)] & PTE_ADDR_MASK) + (virt & 0xFFFULL);
}

void phys_flush_cache(void *addr, uint64_t size)
{
    uintptr_t p   = (uintptr_t)addr & ~63ULL;
    uintptr_t end = (uintptr_t)addr + size;
    while (p < end) { __asm__ volatile("clflush (%0)" :: "r"(p)); p += 64; }
    __asm__ volatile("mfence" ::: "memory");
}

void phys_invalidate_cache(void *addr, uint64_t size)
{
    phys_flush_cache(addr, size);
}

uint64_t *get_kernel_pml4(void)    { return kernel_pml4; }

uint64_t get_current_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & ~0xFFFULL;
}

void load_cr3(uint64_t cr3_phys)
{
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3_phys) : "memory");
    kernel_pml4 = (uint64_t *)phys_to_virt(cr3_phys);
}

void set_kernel_pml4(uint64_t *pml4) { kernel_pml4 = pml4; }

void vmm_init(void)
{
    hhdm_offset = pmm_get_hhdm_offset();
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (uint64_t *)phys_to_virt(cr3 & ~0xFFFULL);
    LOG_OK("VMM initialized successfully\n");
    SERIAL(Info, vmm_init, "VMM initialized successfully\n");
}

uint64_t create_user_stack(uint64_t size)
{
    uint64_t num_pages    = ALIGN_UP(size, PAGE_SIZE) / PAGE_SIZE;
    uint64_t stack_bottom = USER_STACK_TOP - size;
    for (uint64_t i = 0; i < num_pages; i++)
        map_page(stack_bottom + i * PAGE_SIZE, palloc(),
                 PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    return USER_STACK_TOP;
}

uint64_t map_user_stack_with_guard(size_t stack_size)
{
    uint64_t stack_bottom = USER_STACK_TOP - stack_size - GUARD_PAGE_SIZE;
    for (uint64_t addr = stack_bottom + GUARD_PAGE_SIZE;
         addr < USER_STACK_TOP; addr += PAGE_SIZE)
        map_page(addr, palloc(), PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    return USER_STACK_TOP;
}