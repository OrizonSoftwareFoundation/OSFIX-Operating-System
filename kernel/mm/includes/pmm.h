/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin Tantawi
 @license: EUPL 1.2
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <util.h>

#define PAGE_SIZE  4096

#define MAX_ORDER  11

struct PhysicalMemoryRegion {
    struct PhysicalMemoryRegion *next; 

};

void     pmm_init(void);

uint64_t palloc(void);

uint64_t palloc_order(int order);

void     pfree(uint64_t phys_addr);

void     pfree_order(uint64_t phys_addr, int order);

uint64_t pmm_get_total_pages(void);

uint64_t pmm_get_free_pages(void);

uint64_t pmm_get_used_pages(void);

uint64_t pmm_get_hhdm_offset(void);

#endif