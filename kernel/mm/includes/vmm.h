/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin Tantawi
 @license: EUPL 1.2
 */


#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "pmm.h"

#define PTE_PRESENT        (1ULL << 0)  

#define PTE_WRITABLE       (1ULL << 1)  

#define PTE_USER           (1ULL << 2)  

#define PTE_WRITE_THROUGH  (1ULL << 3)  

#define PTE_CACHE_DISABLE  (1ULL << 4)  

#define PTE_ACCESSED       (1ULL << 5)  

#define PTE_DIRTY          (1ULL << 6)  

#define PTE_PS             (1ULL << 7)  

#define PTE_PAT            (1ULL << 7)  

#define PTE_GLOBAL         (1ULL << 8)  

#define PTE_NX             (1ULL << 63) 

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PML4_INDEX(v)  (((uint64_t)(v) >> 39) & 0x1FF) 

#define PDPT_INDEX(v)  (((uint64_t)(v) >> 30) & 0x1FF) 

#define PD_INDEX(v)    (((uint64_t)(v) >> 21) & 0x1FF) 

#define PT_INDEX(v)    (((uint64_t)(v) >> 12) & 0x1FF) 

#define USER_STACK_TOP   0x0000700000000000ULL 

#define GUARD_PAGE_SIZE  PAGE_SIZE             

void    *phys_to_virt(uint64_t phys);

uint64_t virt_to_phys(void *virt);

uint64_t virt_to_phys_user(uint64_t virt);

void vmm_init(void);

void map_page(uint64_t virt, uint64_t phys, uint64_t flags);

void unmap_page(uint64_t virt);

void phys_flush_cache(void *addr, uint64_t size);

void phys_invalidate_cache(void *addr, uint64_t size);

uint64_t  get_current_cr3(void);

void      load_cr3(uint64_t cr3_phys);

uint64_t *get_kernel_pml4(void);

void      set_kernel_pml4(uint64_t *pml4);

uint64_t create_user_stack(uint64_t size);

uint64_t map_user_stack_with_guard(size_t stack_size);

#endif