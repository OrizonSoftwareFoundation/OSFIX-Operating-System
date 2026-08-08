#ifndef KSYMS_H
#define KSYMS_H

#include <stdint.h>

typedef struct {
    uint64_t addr;
    const char *name;
} KSym_t;

extern const KSym_t g_ksyms[];
extern const uint64_t g_ksyms_count;

//nearest symbol at or below "addr", or NULL if the table is empty or addr is below the first symbol
//g_ksyms must be sorted ascending by addr otherwise all hell breaks loose (nm -n guarantees this)
const KSym_t *ksym_find(uint64_t addr);

#endif
//you know i'm starting to get tired of writing headers