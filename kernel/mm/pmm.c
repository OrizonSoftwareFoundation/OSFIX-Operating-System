/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Nora A.
 @license: EUPL 1.2
 */

#include "includes/pmm.h"
#include <limine.h>
#include <kprintf.h>
#include <serial.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

#define LIMINE_MEMMAP_USABLE 0

static uint64_t pmm_total_pages = 0;

static uint64_t pmm_free_pages  = 0;

static uint8_t *pmm_bitmap      = NULL;

static uint64_t pmm_max_page    = 0;

static uint64_t hhdm_offset     = 0;

static struct PhysicalMemoryRegion *free_lists[MAX_ORDER];

static inline void bitmap_set(uint64_t bit)
{
    pmm_bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit)
{
    pmm_bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static inline int bitmap_test(uint64_t bit)
{
    return (pmm_bitmap[bit / 8] >> (bit % 8)) & 1;
}

static void pmm_list_add(int order, uint64_t phys_addr)
{
    struct PhysicalMemoryRegion *region =
        (struct PhysicalMemoryRegion *)(phys_addr + hhdm_offset);
    region->next      = free_lists[order];
    free_lists[order] = region;
}

static void pmm_list_remove(int order, uint64_t phys_addr)
{
    struct PhysicalMemoryRegion **prev = &free_lists[order];
    struct PhysicalMemoryRegion *curr  = *prev;
    while (curr) {
        if ((uint64_t)curr - hhdm_offset == phys_addr) {
            *prev = curr->next;
            return;
        }
        prev = &curr->next;
        curr = *prev;
    }
}

static int pmm_list_contains(int order, uint64_t phys_addr)
{
    struct PhysicalMemoryRegion *curr = free_lists[order];
    while (curr) {
        if ((uint64_t)curr - hhdm_offset == phys_addr)
            return 1;
        curr = curr->next;
    }
    return 0;
}

uint64_t palloc_order(int order)
{
    for (int i = order; i < MAX_ORDER; i++) {
        if (!free_lists[i])
            continue;

        uint64_t addr = (uint64_t)free_lists[i] - hhdm_offset;
        free_lists[i] = free_lists[i]->next;

        while (i > order) {
            i--;
            pmm_list_add(i, addr + ((uint64_t)PAGE_SIZE << i));
        }

        uint64_t page_idx = addr / PAGE_SIZE;
        for (uint64_t j = 0; j < (1ULL << order); j++)
            bitmap_set(page_idx + j);

        pmm_free_pages -= (1ULL << order);
        return addr;
    }
    return 0;
}

uint64_t palloc(void)
{
    return palloc_order(0);
}

void pfree_order(uint64_t phys_addr, int order)
{
    uint64_t page_idx = phys_addr / PAGE_SIZE;

    for (uint64_t j = 0; j < (1ULL << order); j++)
        bitmap_clear(page_idx + j);

    for (; order < MAX_ORDER - 1; order++) {
        uint64_t buddy_idx  = page_idx ^ (1ULL << order);
        uint64_t buddy_phys = buddy_idx * PAGE_SIZE;

        if (buddy_idx >= pmm_max_page || bitmap_test(buddy_idx))
            break;
        if (!pmm_list_contains(order, buddy_phys))
            break;

        pmm_list_remove(order, buddy_phys);
        if (buddy_idx < page_idx)
            page_idx = buddy_idx;
    }

    pmm_list_add(order, page_idx * PAGE_SIZE);
    pmm_free_pages += (1ULL << order);
}

void pfree(uint64_t phys_addr)
{
    pfree_order(phys_addr, 0);
}

void pmm_init(void)
{
    if (!hhdm_request.response) {
        log(Fatal, "PMM: HHDM not available\n");
        for (;;) __asm__ volatile("hlt");
    }
    log(Info, "HHDM offset: %lx\n", hhdm_request.response->offset);

    if (!memmap_request.response) {
        log(Fatal, "PMM: memory map not available\n");
        for (;;) __asm__ volatile("hlt");
    }
    log(Info, "Memory map entries: %d\n", memmap_request.response->entry_count);

    hhdm_offset = hhdm_request.response->offset;
    struct limine_memmap_response *memmap = memmap_request.response;

    uint64_t top_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t end = memmap->entries[i]->base + memmap->entries[i]->length;
        if (end > top_addr) top_addr = end;
    }

    pmm_max_page = top_addr / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(pmm_max_page / 8, PAGE_SIZE);

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE || e->length < bitmap_size)
            continue;
        pmm_bitmap = (uint8_t *)(e->base + hhdm_offset);
        memset(pmm_bitmap, 0xFF, bitmap_size);
        e->base   += bitmap_size;
        e->length -= bitmap_size;
        break;
    }

    if (!pmm_bitmap) {
        log(Fatal, "PMM: failed to place bitmap\n");
        for (;;) __asm__ volatile("hlt");
    }
    log(Info, "PMM bitmap placed at physical address %lx, size %lu bytes\n",
           (uint64_t)pmm_bitmap - hhdm_offset, bitmap_size);

    for (int i = 0; i < MAX_ORDER; i++)
        free_lists[i] = NULL;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t base = ALIGN_UP(e->base, PAGE_SIZE);
        uint64_t end  = ALIGN_DOWN(e->base + e->length, PAGE_SIZE);

        for (uint64_t cur = base; cur < end;) {
            int order = 0;
            while (order < MAX_ORDER - 1) {
                uint64_t size = (uint64_t)PAGE_SIZE << (order + 1);
                if (cur + size > end || cur % size != 0)
                    break;
                order++;
            }

            uint64_t page_idx = cur / PAGE_SIZE;
            for (uint64_t j = 0; j < (1ULL << order); j++)
                bitmap_clear(page_idx + j);

            pmm_list_add(order, cur);
            pmm_total_pages += (1ULL << order);
            pmm_free_pages  += (1ULL << order);
            cur += (uint64_t)PAGE_SIZE << order;
        }
    }

    log(Info, "PMM initialized: %d bytes total, %d bytes free\n",
           (int)(pmm_total_pages * PAGE_SIZE),
           (int)(pmm_free_pages * PAGE_SIZE));
}

uint64_t pmm_get_total_pages(void) { return pmm_total_pages; }
uint64_t pmm_get_free_pages(void)  { return pmm_free_pages;  }
uint64_t pmm_get_used_pages(void)  { return pmm_total_pages - pmm_free_pages; }
uint64_t pmm_get_hhdm_offset(void) { return hhdm_offset; }