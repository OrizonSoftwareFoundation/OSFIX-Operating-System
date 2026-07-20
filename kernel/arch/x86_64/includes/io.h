#ifndef IO_H
#define IO_H

#define UNUSED_PORT 0x80

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <util.h>
#include <pmm.h>

uint8_t inb(uint16_t port);

void outb(uint16_t port, uint8_t val);

typedef uint64_t virt_addr_t;

typedef uint64_t physc_addr_t;

static inline void hcf(void) {
    for (;;) {
        __asm__("hlt");
    }
}

static inline void iowait() {
    outb(UNUSED_PORT, 0);
}

static inline void cpuGetMSR(uint32_t msr, uint32_t *eax, uint32_t *edx) {
    __asm__ (
        "rdmsr"
        : "=a"(*eax), "=d"(*edx)
        : "c"(msr)
    );
}

static inline void cpuSetMSR(uint32_t msr, uint32_t eax, uint32_t edx) {
    __asm__ (
        "wrmsr"
        :
        : "c"(msr), "a"(eax), "d"(edx)
    );
}

typedef struct list_t {
    struct list_t *next, *prev; 

} list_t;

static inline void list_init(list_t *list) {
    list->next = list;
    list->prev = list;
}

static inline list_t *list_last(list_t *list) {
    return list->prev != list ? list->prev : NULL;
}

static inline list_t *list_next(list_t *list) {
    return list->next != list ? list->next : NULL;
}

static inline void list_insert(list_t *new, list_t *link) {
    new->prev = link->prev;
    new->next = link;
    new->prev->next = new;
    new->next->prev = new;
}

static inline void list_append(list_t *new, list_t *into) {
    list_insert(new, into);
}

static inline void list_remove(list_t *list) {
    list->prev->next = list->next;
    list->next->prev = list->prev;
    list->next = list;
    list->prev = list;
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void set_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(val));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void insw(uint16_t port, void *buf, unsigned long count) {
    __asm__ __volatile__ (
        "cld; rep insw"
        : "+D"(buf), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static inline void outsw(uint16_t port, const void *buf, unsigned long count) {
    __asm__ __volatile__ (
        "cld; rep outsw"
        : "+S"(buf), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

void enable_interrupts();

void disable_interrupts();

void halt_interrupts_enabled(void);

void halt(void);

void cpuid(uint32_t leaf, uint32_t subleaf,
           uint32_t *eax, uint32_t *ebx,
           uint32_t *ecx, uint32_t *edx);

#endif
