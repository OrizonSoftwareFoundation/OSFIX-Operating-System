/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "includes/main/util/elf/elf_loader.h"
#include "includes/main/util/elf/elf.h"
#include "includes/main/fsm/vfs.h"
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <serial.h>
#include <stdlib.h>
#include <string.h>
#include <kprintf.h>
#include "includes/main/module/ksym.h"
#include "includes/main/module/modsym.h"

static void map_segment(uintptr_t vaddr, size_t memsz, size_t filesz,
                        const unsigned char *filedata, size_t filedata_len,
                        uintptr_t file_offset)
{
    uintptr_t vaddr_aligned  = vaddr & ~(PAGE_SIZE - 1);
    size_t    off_in_page    = vaddr & (PAGE_SIZE - 1);
    size_t    num_pages      = (memsz + off_in_page + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t p = 0; p < num_pages; p++) {
        uintptr_t curr_vaddr = vaddr_aligned + p * PAGE_SIZE;
        uint64_t  phys       = palloc();
        map_page(curr_vaddr, phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);

        void  *dest      = phys_to_virt(phys);
        memset(dest, 0, PAGE_SIZE);

        size_t copy_off = (p == 0) ? off_in_page : 0;
        size_t copy_len = PAGE_SIZE - copy_off;

        intptr_t file_offset_in_seg = (intptr_t)(p * PAGE_SIZE - off_in_page);
            if (p == 0) file_offset_in_seg = 0;

            if (file_offset_in_seg < 0 || (size_t)file_offset_in_seg >= filesz) continue;

            intptr_t file_rem = (intptr_t)filesz - file_offset_in_seg;
            if (file_rem <= 0) continue;

            size_t actual_copy = (size_t)file_rem > copy_len ? copy_len : (size_t)file_rem;
            size_t src_off     = file_offset + file_offset_in_seg;
            if (src_off + actual_copy <= filedata_len)
                memcpy((unsigned char *)dest + copy_off,
                       filedata + src_off,
                       actual_copy);
    }
}

static void setup_stack(uintptr_t stack_base, size_t stack_pages)
{
    for (size_t i = 0; i < stack_pages; i++)
        map_page(stack_base - (i + 1) * PAGE_SIZE,
                 palloc(),
                 PTE_PRESENT | PTE_WRITABLE | PTE_USER);
}

static int elf64_load_path(vfs_mount_t *mnt, uint64_t inode,
                           Elf64_Ehdr *ehdr, elf_info_t *info, uint64_t stack_top)
{
    size_t    ph_size = ehdr->e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = malloc(ph_size);
    if (!phdrs) return -1;
    vfs_read(mnt, inode, (unsigned char *)phdrs, ph_size, ehdr->e_phoff);

    uintptr_t load_base = 0;
    if (ehdr->e_type == ET_DYN) {
        load_base = 0x400000;
    } else {
        uintptr_t min_vaddr = UINTPTR_MAX;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr)
                min_vaddr = phdrs[i].p_vaddr;
        }
        if (min_vaddr < 0x400000) {
            load_base = 0x400000 - min_vaddr;
        }
    }

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uintptr_t vaddr  = phdrs[i].p_vaddr + load_base;
        size_t    filesz = phdrs[i].p_filesz;
        size_t    memsz  = phdrs[i].p_memsz;

        serial_printf("ELF64: PT_LOAD vaddr=%p filesz=%llu memsz=%llu\n",
                      vaddr, filesz, memsz);

        uintptr_t vaddr_aligned = vaddr & ~(PAGE_SIZE - 1);
        size_t    off_in_page   = vaddr & (PAGE_SIZE - 1);
        size_t    num_pages     = (memsz + off_in_page + PAGE_SIZE - 1) / PAGE_SIZE;

        for (size_t p = 0; p < num_pages; p++) {
            uintptr_t curr_vaddr = vaddr_aligned + p * PAGE_SIZE;
            uint64_t  phys       = palloc();
            uint64_t map_flags = PTE_PRESENT | PTE_WRITABLE;
            if (!info->privileged) map_flags |= PTE_USER;
            map_page(curr_vaddr, phys, map_flags);

            void  *dest     = phys_to_virt(phys);
            memset(dest, 0, PAGE_SIZE);

            size_t copy_off = (p == 0) ? off_in_page : 0;
            size_t copy_len = PAGE_SIZE - copy_off;

            intptr_t file_offset_in_seg = (intptr_t)(p * PAGE_SIZE - off_in_page);
            if (p == 0) file_offset_in_seg = 0;

            if (file_offset_in_seg < 0 || (size_t)file_offset_in_seg >= filesz) continue;

            intptr_t file_rem = (intptr_t)filesz - file_offset_in_seg;
            if (file_rem <= 0) continue;

            size_t actual_copy = (size_t)file_rem > copy_len ? copy_len : (size_t)file_rem;
            vfs_read(mnt, inode,
                     (unsigned char *)dest + copy_off,
                     actual_copy,
                     phdrs[i].p_offset + file_offset_in_seg);
        }
    }

    setup_stack(stack_top, 16);

    uintptr_t entry = ehdr->e_entry;
    if (load_base != 0) {
        entry = ehdr->e_entry + load_base;
    } else if (ehdr->e_entry < 0x400000) {
        uintptr_t min_vaddr = UINTPTR_MAX;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr)
                min_vaddr = phdrs[i].p_vaddr;
        }
        if (min_vaddr >= 0x400000) {
            entry = ehdr->e_entry + 0x400000;
        }
    }

    info->entry     = entry;
    info->stack_top = stack_top;
    info->phnum     = ehdr->e_phnum;
    info->phent     = ehdr->e_phentsize;
    info->phoff     = ehdr->e_phoff;
    info->type      = ehdr->e_type;
    free(phdrs);
    return 0;
}

static int elf32_load_path(vfs_mount_t *mnt, uint64_t inode,
                           Elf32_Ehdr *ehdr, elf_info_t *info, uint64_t stack_top)
{
    size_t     ph_size = ehdr->e_phnum * sizeof(Elf32_Phdr);
    Elf32_Phdr *phdrs  = malloc(ph_size);
    if (!phdrs) return -1;
    vfs_read(mnt, inode, (unsigned char *)phdrs, ph_size, ehdr->e_phoff);

    uintptr_t load_base = 0;
    if (ehdr->e_type == ET_DYN) {
        load_base = 0x400000;
    } else {
        uintptr_t min_vaddr = UINTPTR_MAX;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr)
                min_vaddr = phdrs[i].p_vaddr;
        }
        if (min_vaddr < 0x400000) {
            load_base = 0x400000 - min_vaddr;
        } else if (ehdr->e_entry < 0x400000 && min_vaddr >= 0x400000) {
            load_base = 0x400000;
        }
    }

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uintptr_t vaddr  = (uintptr_t)phdrs[i].p_vaddr + load_base;
        size_t    filesz = phdrs[i].p_filesz;
        size_t    memsz  = phdrs[i].p_memsz;

        serial_printf("ELF32: PT_LOAD vaddr=%p filesz=%u memsz=%u\n",
                      vaddr, (uint32_t)filesz, (uint32_t)memsz);

        uintptr_t vaddr_aligned = vaddr & ~(PAGE_SIZE - 1);
        size_t    off_in_page   = vaddr & (PAGE_SIZE - 1);
        size_t    num_pages     = (memsz + off_in_page + PAGE_SIZE - 1) / PAGE_SIZE;

        for (size_t p = 0; p < num_pages; p++) {
            uintptr_t curr_vaddr = vaddr_aligned + p * PAGE_SIZE;
            uint64_t  phys       = palloc();
            uint64_t map_flags = PTE_PRESENT | PTE_WRITABLE;
            if (!info->privileged) map_flags |= PTE_USER;
            map_page(curr_vaddr, phys, map_flags);

            void  *dest     = phys_to_virt(phys);
            memset(dest, 0, PAGE_SIZE);

            size_t copy_off = (p == 0) ? off_in_page : 0;
            size_t copy_len = PAGE_SIZE - copy_off;

            intptr_t file_offset_in_seg = (intptr_t)(p * PAGE_SIZE - off_in_page);
            if (p == 0) file_offset_in_seg = 0;

            if (file_offset_in_seg < 0 || (size_t)file_offset_in_seg >= filesz) continue;

            intptr_t file_rem = (intptr_t)filesz - file_offset_in_seg;
            if (file_rem <= 0) continue;

            size_t actual_copy = (size_t)file_rem > copy_len ? copy_len : (size_t)file_rem;
            vfs_read(mnt, inode,
                     (unsigned char *)dest + copy_off,
                     actual_copy,
                     phdrs[i].p_offset + file_offset_in_seg);
        }
    }

    setup_stack(stack_top, 16);

    uintptr_t entry = ehdr->e_entry;
    if (load_base != 0) {
        entry = ehdr->e_entry + load_base;
    } else if (ehdr->e_entry < 0x400000) {
        uintptr_t min_vaddr = UINTPTR_MAX;
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr)
                min_vaddr = phdrs[i].p_vaddr;
        }
        if (min_vaddr >= 0x400000) {
            entry = ehdr->e_entry + 0x400000;
        }
    }

    info->entry     = entry;
    info->stack_top = stack_top;
    info->phnum     = ehdr->e_phnum;
    info->phent     = ehdr->e_phentsize;
    info->phoff     = ehdr->e_phoff;
    info->type      = ehdr->e_type;
    free(phdrs);
    return 0;
}

int elf_load(const char *path, elf_info_t *info, uint64_t stack_top)
{
    vfs_mount_t *mnt = vfs_get_root_mount();
    if (!mnt) {
        serial_printf("ELF: No root mount\n");
        return -1;
    }

    uint64_t inode = vfs_path_to_inode(mnt, path);
    if (!inode) {
        serial_printf("ELF: File not found: %s\n", path);
        return -1;
    }

    unsigned char hdr_buf[sizeof(Elf64_Ehdr)];
    if (vfs_read(mnt, inode, hdr_buf, sizeof(hdr_buf), 0) != sizeof(hdr_buf)) {
        serial_printf("ELF: Failed to read header\n");
        serial_printf("ELF: Header bytes:");
        for (int i = 0; i < 16; i++) serial_printf(" %x", hdr_buf[i]);
        serial_printf("\n");
        return -1;
    }

    if (memcmp(hdr_buf, "\x7f\x45\x4c\x46", 4) != 0) {
        serial_printf("ELF: Invalid magic\n");
        return -1;
    }

    uint8_t class = hdr_buf[4]; 

    if (class == ELFCLASS64) {
        Elf64_Ehdr *ehdr = (Elf64_Ehdr *)hdr_buf;
        if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
            serial_printf("ELF64: Not executable (type %d)\n", ehdr->e_type);
            return -1;
        }
        serial_printf("ELF: Loading 64-bit %s entry=%p phnum=%d\n",
                      path, ehdr->e_entry, ehdr->e_phnum);
        info->is_32bit = 0;
        return elf64_load_path(mnt, inode, ehdr, info, stack_top);

    } else if (class == ELFCLASS32) {
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)hdr_buf;
        if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
            serial_printf("ELF32: Not executable (type %d)\n", ehdr->e_type);
            return -1;
        }
        serial_printf("ELF: Loading 32-bit %s entry=%p phnum=%d\n",
                      path, (uintptr_t)ehdr->e_entry, ehdr->e_phnum);
        info->is_32bit = 1;
        return elf32_load_path(mnt, inode, ehdr, info, stack_top);

    } else {
        serial_printf("ELF: Unknown class %d\n", class);
        return -1;
    }
}

int elf_load_from_memory(uintptr_t address, size_t size, elf_info_t *info)
{
    if (size < sizeof(Elf64_Ehdr)) {
        serial_printf("ELF: File too small\n");
        return -1;
    }

    unsigned char *buf = (unsigned char *)address;

    if (memcmp(buf, "\x7f\x45\x4c\x46", 4) != 0) {
        serial_printf("ELF: Invalid magic\n");
        return -1;
    }

    uint8_t class = buf[4];

    if (class == ELFCLASS64) {
        Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
        if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
            serial_printf("ELF64: Not executable (type %d)\n", ehdr->e_type);
            return -1;
        }
        if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr) > size) {
            serial_printf("ELF64: PHDRs outside file bounds\n");
            return -1;
        }

        Elf64_Phdr *phdrs    = (Elf64_Phdr *)(buf + ehdr->e_phoff);
        uintptr_t  load_base;
        if (ehdr->e_type == ET_DYN) {
            load_base = 0x400000;
        } else {
            uintptr_t min_vaddr = UINTPTR_MAX;
            uintptr_t max_vaddr = 0;
            for (int i = 0; i < ehdr->e_phnum; i++) {
                if (phdrs[i].p_type == PT_LOAD) {
                    if (phdrs[i].p_vaddr < min_vaddr)
                        min_vaddr = phdrs[i].p_vaddr;
                    uintptr_t seg_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
                    if (seg_end > max_vaddr)
                        max_vaddr = seg_end;
                }
            }
            if (ehdr->e_entry >= min_vaddr && ehdr->e_entry < max_vaddr) {
                load_base = 0;
            } else if (ehdr->e_entry < min_vaddr) {
                load_base = 0x400000 - min_vaddr;
            } else {
                load_base = (min_vaddr < 0x400000) ? 0x400000 - min_vaddr : 0;
            }
        }

        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type != PT_LOAD) continue;
            uintptr_t vaddr  = phdrs[i].p_vaddr + load_base;
            size_t    filesz = phdrs[i].p_filesz;
            size_t    memsz  = phdrs[i].p_memsz;
            serial_printf("ELF64: PT_LOAD vaddr=%p filesz=%llu memsz=%llu\n",
                          vaddr, filesz, memsz);
            if (phdrs[i].p_offset + filesz > size) {
                serial_printf("ELF64: Segment outside file bounds\n");
                return -1;
            }
            map_segment(vaddr, memsz, filesz, buf, size, phdrs[i].p_offset);
        }

        uintptr_t entry = ehdr->e_entry;
        if (load_base != 0) {
            entry = ehdr->e_entry + load_base;
        } else if (ehdr->e_entry < 0x400000) {
            entry = ehdr->e_entry + 0x400000;
        }

        info->entry     = entry;
        info->stack_top = 0x700000000000;
        info->is_32bit  = 0;
        info->phnum     = ehdr->e_phnum;
        info->phent     = ehdr->e_phentsize;
        info->phoff     = ehdr->e_phoff;
        info->type      = ehdr->e_type;
        serial_printf("ELF64: Loaded from memory, entry=%p\n", info->entry);
        return 0;

    } else if (class == ELFCLASS32) {
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)buf;
        if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
            serial_printf("ELF32: Not executable (type %d)\n", ehdr->e_type);
            return -1;
        }
        if (ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf32_Phdr) > size) {
            serial_printf("ELF32: PHDRs outside file bounds\n");
            return -1;
        }

        Elf32_Phdr *phdrs    = (Elf32_Phdr *)(buf + ehdr->e_phoff);
        uintptr_t  load_base;
        if (ehdr->e_type == ET_DYN) {
            load_base = 0x400000;
        } else {
            uintptr_t min_vaddr = UINTPTR_MAX;
            uintptr_t max_vaddr = 0;
            for (int i = 0; i < ehdr->e_phnum; i++) {
                if (phdrs[i].p_type == PT_LOAD) {
                    if (phdrs[i].p_vaddr < min_vaddr)
                        min_vaddr = phdrs[i].p_vaddr;
                    uintptr_t seg_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
                    if (seg_end > max_vaddr)
                        max_vaddr = seg_end;
                }
            }
            if (ehdr->e_entry >= min_vaddr && ehdr->e_entry < max_vaddr) {
                load_base = 0;
            } else if (ehdr->e_entry < min_vaddr) {
                load_base = 0x400000 - min_vaddr;
            } else {
                load_base = (min_vaddr < 0x400000) ? 0x400000 - min_vaddr : 0;
            }
        }

        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type != PT_LOAD) continue;
            uintptr_t vaddr  = (uintptr_t)phdrs[i].p_vaddr + load_base;
            size_t    filesz = phdrs[i].p_filesz;
            size_t    memsz  = phdrs[i].p_memsz;
            serial_printf("ELF32: PT_LOAD vaddr=%p filesz=%u memsz=%u\n",
                          vaddr, (uint32_t)filesz, (uint32_t)memsz);
            if (phdrs[i].p_offset + filesz > size) {
                serial_printf("ELF32: Segment outside file bounds\n");
                return -1;
            }
            map_segment(vaddr, memsz, filesz, buf, size, phdrs[i].p_offset);
        }

        uintptr_t stack_base = 0xC0000000;
        setup_stack(stack_base, 16);

        uintptr_t entry = ehdr->e_entry;
        if (load_base != 0) {
            entry = ehdr->e_entry + load_base;
        } else if (ehdr->e_entry < 0x400000) {
            uintptr_t min_vaddr = UINTPTR_MAX;
            for (int i = 0; i < ehdr->e_phnum; i++) {
                if (phdrs[i].p_type == PT_LOAD && phdrs[i].p_vaddr < min_vaddr)
                    min_vaddr = phdrs[i].p_vaddr;
            }
            if (min_vaddr >= 0x400000) {
                entry = ehdr->e_entry + 0x400000;
            }
        }

        info->entry     = entry;
        info->stack_top = stack_base;
        info->is_32bit  = 1;
        info->phnum     = ehdr->e_phnum;
        info->phent     = ehdr->e_phentsize;
        info->phoff     = ehdr->e_phoff;
        info->type      = ehdr->e_type;
        serial_printf("ELF32: Loaded from memory, entry=%p\n", info->entry);
        return 0;

    } else {
        serial_printf("ELF: Unknown class %d\n", class);
        return -1;
    }
}

static inline size_t mod_align_up(size_t val, size_t align)
{
    if (align <= 1) return val;
    return (val + align - 1) & ~(align - 1);
}

static int mod_resolve_sym(uint8_t *image,
                            Elf64_Ehdr *ehdr,
                            Elf64_Shdr *shdrs,
                            const Elf64_Sym *sym,
                            const char *strtab,
                            uint64_t *out)
{
if (sym->st_shndx == SHN_UNDEF) {
    const char *name = strtab + sym->st_name;
    uintptr_t addr = ksym_lookup(name);
    if (!addr) addr = modsym_lookup(name);
    if (!addr) {
        serial_printf("module: undefined symbol '%s'\n", name);
        return -1;
    }
    *out = (uint64_t)addr;
    return 0;
}
    if (sym->st_shndx == SHN_ABS) {
        *out = (uint64_t)sym->st_value;
        return 0;
    }

    if (sym->st_shndx >= ehdr->e_shnum) {
        serial_printf("module: symbol shndx %u out of range\n",
                      (unsigned)sym->st_shndx);
        return -1;
    }

    *out = (uint64_t)image
           + shdrs[sym->st_shndx].sh_addr
           + sym->st_value;
    return 0;
}

static void mod_apply_rela(uint8_t *patch, uint64_t S, int64_t A,
                            uint32_t r_type)
{
    uint64_t P = (uint64_t)patch;

    switch (r_type) {
    case R_X86_64_NONE:
        break;
    case R_X86_64_64:
        *(uint64_t *)patch = S + (uint64_t)A;
        break;
    case R_X86_64_32:
        *(uint32_t *)patch = (uint32_t)(S + (uint64_t)A);
        break;
    case R_X86_64_32S:
        *(int32_t *)patch  = (int32_t)(S + (uint64_t)A);
        break;
    case R_X86_64_PC32:
    case R_X86_64_PLT32:
        *(int32_t *)patch  = (int32_t)((int64_t)(S + (uint64_t)A) - (int64_t)P);
        break;
    default:
        serial_printf("module: unhandled reloc type %u at %p\n",
                      r_type, (void *)patch);
        break;
    }
}

int elf_load_module(const char *path, module_load_info_t *out)
{
    serial_printf("elf_load_module: attempting '%s'\n", path);
    vfs_mount_t *mnt = vfs_get_root_mount();
    if (!mnt) {
        serial_printf("module: no root mount\n");
        return -1;
    }

    uint64_t inode = vfs_path_to_inode(mnt, path);
    if (!inode) {
        serial_printf("module: not found: %s\n", path);
        return -1;
    }

    vfs_stat_t st;
    if (vfs_stat(mnt, inode, &st) != 0 || st.size == 0) {
        serial_printf("module: stat failed: %s\n", path);
        return -1;
    }

    uint8_t *buf = malloc(st.size);
    if (!buf) {
        serial_printf("module: out of memory\n");
        return -1;
    }

    if ((size_t)vfs_read(mnt, inode, buf, st.size, 0) != st.size) {
        serial_printf("module: short read: %s\n", path);
        free(buf);
        return -1;
    }

    if (st.size < sizeof(Elf64_Ehdr)) {
        serial_printf("module: file too small: %s\n", path);
        free(buf);
        return -1;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;

    if (memcmp(ehdr->e_ident, "\x7f\x45\x4c\x46", 4) != 0) {
        serial_printf("module: bad magic: %s\n", path);
        free(buf);
        return -1;
    }
    if (ehdr->e_ident[4] != ELFCLASS64) {
        serial_printf("module: not 64-bit: %s\n", path);
        free(buf);
        return -1;
    }
    if (ehdr->e_type != ET_REL) {
        serial_printf("module: not ET_REL (type=%u): %s\n",
                      (unsigned)ehdr->e_type, path);
        free(buf);
        return -1;
    }
    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) {
        serial_printf("module: no section headers: %s\n", path);
        free(buf);
        return -1;
    }

    Elf64_Shdr *shdrs = (Elf64_Shdr *)(buf + ehdr->e_shoff);

    size_t image_size = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (!(shdrs[i].sh_flags & SHF_ALLOC)) continue;
        image_size = mod_align_up(image_size, (size_t)shdrs[i].sh_addralign);
        shdrs[i].sh_addr = (Elf64_Addr)image_size;
        image_size += (size_t)shdrs[i].sh_size;
    }

    if (image_size == 0) {
        serial_printf("module: no loadable sections: %s\n", path);
        free(buf);
        return -1;
    }

    uint8_t *image = malloc(image_size);
    if (!image) {
        serial_printf("module: out of memory for image (%zu bytes)\n",
                      image_size);
        free(buf);
        return -1;
    }
    memset(image, 0, image_size);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (!(shdrs[i].sh_flags & SHF_ALLOC)) continue;
        if (shdrs[i].sh_type == SHT_NOBITS) continue; 
        memcpy(image + shdrs[i].sh_addr,
               buf   + shdrs[i].sh_offset,
               (size_t)shdrs[i].sh_size);
    }

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type != SHT_RELA) continue;

        uint32_t target_idx = shdrs[i].sh_info;
        uint32_t symtab_idx = shdrs[i].sh_link;
        if (target_idx >= ehdr->e_shnum || symtab_idx >= ehdr->e_shnum)
            continue;

        Elf64_Shdr *target      = &shdrs[target_idx];
        Elf64_Shdr *symtab_shdr = &shdrs[symtab_idx];
        if (!(target->sh_flags & SHF_ALLOC)) continue;

        uint32_t strtab_idx = symtab_shdr->sh_link;
        if (strtab_idx >= ehdr->e_shnum) continue;

        Elf64_Sym  *symtab = (Elf64_Sym  *)(buf + symtab_shdr->sh_offset);
        const char *strtab = (const char *)(buf + shdrs[strtab_idx].sh_offset);
        Elf64_Rela *relas  = (Elf64_Rela *)(buf + shdrs[i].sh_offset);
        size_t      nrela  = (size_t)shdrs[i].sh_size / sizeof(Elf64_Rela);

        for (size_t j = 0; j < nrela; j++) {
            uint32_t sym_idx = (uint32_t)ELF64_R_SYM(relas[j].r_info);
            uint32_t r_type  = (uint32_t)ELF64_R_TYPE(relas[j].r_info);

            uint64_t sym_val = 0;
            if (sym_idx != 0) {
                if (mod_resolve_sym(image, ehdr, shdrs, &symtab[sym_idx],
                                    strtab, &sym_val) != 0) {
                    free(image);
                    free(buf);
                    return -1;
                }
            }

            uint8_t *patch = image + target->sh_addr + relas[j].r_offset;
            mod_apply_rela(patch, sym_val,
                           (int64_t)relas[j].r_addend, r_type);
        }
    }

    uintptr_t entry = 0;

    for (int i = 0; i < ehdr->e_shnum && !entry; i++) {
        if (shdrs[i].sh_type != SHT_SYMTAB) continue;

        Elf64_Sym  *syms   = (Elf64_Sym  *)(buf + shdrs[i].sh_offset);
        const char *strtab = (const char *)(buf + shdrs[shdrs[i].sh_link].sh_offset);
        size_t      nsym   = (size_t)shdrs[i].sh_size / sizeof(Elf64_Sym);

        for (size_t j = 0; j < nsym; j++) {
            if (syms[j].st_shndx == SHN_UNDEF) continue;
            if (syms[j].st_shndx >= ehdr->e_shnum) continue;
            if (strcmp(strtab + syms[j].st_name, "module_init") != 0) continue;

            entry = (uintptr_t)image
                    + shdrs[syms[j].st_shndx].sh_addr
                    + syms[j].st_value;
            break;
        }
    }

    if (!entry) {
        serial_printf("module: no 'module_init' in %s\n", path);
        free(image);
        free(buf);
        return -1;
    }

    modsym_register(buf, image, ehdr, shdrs);
    free(buf);

    out->image      = image;
    out->image_size = image_size;
    out->entry      = entry;

    serial_printf("module: loaded '%s'  image=%p  entry=%p  size=%zu\n",
                  path, (void *)image, (void *)entry, image_size);
    return 0;
}