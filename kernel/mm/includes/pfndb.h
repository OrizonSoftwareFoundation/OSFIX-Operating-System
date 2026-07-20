//Copyright (C) Kevin Alavik
//License: Apache License 2.0
//Source: https://github.com/kevinalavik/lyr/blob/main/kernel/include/mm/pfndb.h

//Edited by Yazin T. for better compatibility with OSFIX
#ifndef _LYR_MM_PFNDB_H
#define _LYR_MM_PFNDB_H

#include <limine.h>
#include <pmm.h>

#define PAGE_FREE (1u << 0)     

#define PAGE_USED (1u << 1)     

#define PAGE_RESERVED (1u << 2) 

#define PAGE_SHARED \
	(1u << 3) 

#define PAGE_COW (1u << 4)    

#define PAGE_POISON (1u << 5) 

typedef struct page page_t;

typedef struct page {
	union {
		page_t *next; 

	} u1;

	union {
		page_t *prev;        

		uint64_t sharecount; 

	} u2;

	uint64_t flags;    

	uint32_t refcount; 

} __attribute__((aligned(64))) page_t;

void pfndb_init(struct limine_memmap_response *memmap);

page_t *pfndb_getdb(void);

uint64_t pfndb_getmax(void);

uint64_t pfndb_pfnaddr(uint64_t pfn);

page_t *pfndb_getptr(uint64_t pfn);

uint64_t pfndb_getpfn(page_t *page);

page_t *pfndb_phys_to_page(uint64_t phys);

uint64_t pfndb_page_to_phys(page_t *page);

void pfndb_dump(void);

#endif