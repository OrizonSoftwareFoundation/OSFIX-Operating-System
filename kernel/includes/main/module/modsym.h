/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef MODSYM_H
#define MODSYM_H

#include <stdint.h>
#include <stddef.h>
#include "../util/elf/elf.h"

void modsym_register(uint8_t *buf, uint8_t *image,
                     Elf64_Ehdr *ehdr, Elf64_Shdr *shdrs);

uintptr_t modsym_lookup(const char *name);

void modsym_dump(void);

#endif