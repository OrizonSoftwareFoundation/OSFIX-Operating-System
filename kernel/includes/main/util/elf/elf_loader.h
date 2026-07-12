/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef ELF_LOADER_H
#define ELF_LOADER_H
#include <stdint.h>
#include <stddef.h>
#include "elf.h"

typedef struct {
    uintptr_t entry;      

    uintptr_t stack_top;  

    int       is_32bit;   

    uint64_t  phnum;      

    uint64_t  phent;      

    uint64_t  phoff;      

    uint32_t  type;       

    int       privileged; 

} elf_info_t;

int elf_load(const char *path, elf_info_t *info, uint64_t stack_top);

int elf_load_from_memory(uintptr_t address, size_t size, elf_info_t *info);

typedef struct {
    void     *image;       

    size_t    image_size;  

    uintptr_t entry;       

} module_load_info_t;

int elf_load_module(const char *path, module_load_info_t *out);

#endif