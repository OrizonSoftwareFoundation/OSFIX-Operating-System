/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "includes/main/module/modsym.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <serial.h>

typedef struct {
    char     *name;   
    uintptr_t addr;   
} modsym_entry_t;

static modsym_entry_t *sym_table = NULL;
static size_t          sym_count = 0;
static size_t          sym_cap   = 0;

static void push_sym(const char *name, uintptr_t addr)
{
    if (sym_count >= sym_cap) {
        size_t new_cap = sym_cap ? sym_cap * 2 : 64;
        modsym_entry_t *grown = realloc(sym_table,
                                        new_cap * sizeof(modsym_entry_t));
        if (!grown) {
            serial_printf("modsym: out of memory growing table\n");
            return;
        }
        sym_table = grown;
        sym_cap   = new_cap;
    }

    char *name_copy = malloc(strlen(name) + 1);
    if (!name_copy) {
        serial_printf("modsym: out of memory copying name '%s'\n", name);
        return;
    }
    strcpy(name_copy, name);

    sym_table[sym_count].name = name_copy;
    sym_table[sym_count].addr = addr;
    sym_count++;
}

void modsym_register(uint8_t *buf, uint8_t *image,
                     Elf64_Ehdr *ehdr, Elf64_Shdr *shdrs)
{

    serial_printf("modsym_register called, shnum=%u\n", ehdr->e_shnum);
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type != SHT_SYMTAB) continue;
        serial_printf("modsym: found SHT_SYMTAB at section %d\n", i);
        Elf64_Sym  *syms   = (Elf64_Sym  *)(buf + shdrs[i].sh_offset);
        const char *strtab = (const char *)(buf +
                              shdrs[shdrs[i].sh_link].sh_offset);
        size_t      nsym   = (size_t)shdrs[i].sh_size / sizeof(Elf64_Sym);

        for (size_t j = 1; j < nsym; j++) { 
            Elf64_Sym *sym = &syms[j];

            if (sym->st_shndx == SHN_UNDEF)  continue;
            if (sym->st_shndx == SHN_ABS)     continue;
            if (ELF64_ST_BIND(sym->st_info) == 0) continue;
            if (sym->st_shndx >= ehdr->e_shnum)  continue;

            const char *name = strtab + sym->st_name;
            if (!name || name[0] == '\0')        continue;

            uintptr_t addr = (uintptr_t)image
                             + shdrs[sym->st_shndx].sh_addr
                             + sym->st_value;

            push_sym(name, addr);
            serial_printf("modsym: exported '%s' @ %p\n", name, (void *)addr);
        }

        break; 
    }
}

uintptr_t modsym_lookup(const char *name)
{
        serial_printf("modsym_lookup: looking for '%s', table has %zu entries\n",
                  name, sym_count);
    for (size_t i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0)
            return sym_table[i].addr;
    }
    return 0;
}

void modsym_dump(void)
{
    serial_printf("modsym: %zu symbols registered\n", sym_count);
    for (size_t i = 0; i < sym_count; i++) {
        serial_printf("  [%zu] %s = %p\n", i,
                      sym_table[i].name,
                      (void *)sym_table[i].addr);
    }
}