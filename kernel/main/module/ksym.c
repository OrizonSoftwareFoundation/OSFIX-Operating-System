/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */
 
#include "includes/main/module/ksym.h"
#include <stddef.h>
#include <string.h>
#include <kprintf.h>
#include <stdlib.h>
#include <serial.h>
#include <cpu/io.h>
#include <time/ktime.h>
#include <log.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <includes/main/cpu/isr.h> 
#include <includes/main/fsm/vfs.h> 
typedef struct {
    const char *name;
    uintptr_t   addr;
} ksym_entry_t;

static const ksym_entry_t ksym_table[] = {

    { "kprintf",      (uintptr_t)kprintf      },
    { "serial_printf",(uintptr_t)serial_printf },
    { "malloc",       (uintptr_t)malloc       },
    { "calloc",       (uintptr_t)calloc       },
    { "realloc",      (uintptr_t)realloc      },
    { "free",         (uintptr_t)free         },
    { "memcpy",       (uintptr_t)memcpy       },
    { "memset",       (uintptr_t)memset       },
    { "memcmp",       (uintptr_t)memcmp       },
    { "strlen",       (uintptr_t)strlen       },
    { "strcpy",       (uintptr_t)strcpy       },
    { "strncpy",      (uintptr_t)strncpy      },
    { "strcmp",       (uintptr_t)strcmp       },
    { "strncmp",      (uintptr_t)strncmp      },
    { "strstr",       (uintptr_t)strstr       },
    { "strchr",       (uintptr_t)strchr       },
    { "serial_write",  (uintptr_t)serial_write  }, 
    { "outb",         (uintptr_t)outb         },
    { "outw",         (uintptr_t)outw         },
    { "outl",         (uintptr_t)outl         },
    { "inb",          (uintptr_t)inb          },
    { "inw",          (uintptr_t)inw          },
    { "inl",          (uintptr_t)inl          },
    { "log_to_terminal", (uintptr_t)log_to_terminal }, 
    { "palloc_order",    (uintptr_t)palloc_order    },
    { "pfree_order",     (uintptr_t)pfree_order     },   
    { "palloc", (uintptr_t)palloc },
    { "phys_to_virt",    (uintptr_t)phys_to_virt    },
    { "virt_to_phys",    (uintptr_t)virt_to_phys    },
    { "phys_flush_cache",    (uintptr_t)phys_flush_cache    },
    { "phys_invalidate_cache", (uintptr_t)phys_invalidate_cache },
    { "udelay",          (uintptr_t)udelay          },
    { "pfree",  (uintptr_t)pfree  },
    { "map_page",        (uintptr_t)map_page        },
    { "get_time_ms", (uintptr_t)get_time_ms },
    { "ISR_RegisterHandler", (uintptr_t)ISR_RegisterHandler }, 
    { "strrchr", (uintptr_t)strrchr },
    { "vfs_register_filesystem", (uintptr_t)vfs_register_filesystem },
{ "vfs_mount",               (uintptr_t)vfs_mount               },
{ "vfs_unmount",             (uintptr_t)vfs_unmount             },
{ "vfs_get_root_mount",      (uintptr_t)vfs_get_root_mount      },
{ "vfs_read",                (uintptr_t)vfs_read                },
{ "vfs_write",               (uintptr_t)vfs_write               },
{ "vfs_listdir",             (uintptr_t)vfs_listdir             },
{ "vfs_path_to_inode",       (uintptr_t)vfs_path_to_inode       },
{ "vfs_create",              (uintptr_t)vfs_create              },
{ "vfs_mkdir",               (uintptr_t)vfs_mkdir               },
{ "vfs_remove",              (uintptr_t)vfs_remove              },
{ "vfs_rename",              (uintptr_t)vfs_rename              },
{ "vfs_stat",                (uintptr_t)vfs_stat                },
{ "vfs_get_mount_for_path",  (uintptr_t)vfs_get_mount_for_path  },

    { NULL, 0 } 
};

uintptr_t ksym_lookup(const char *name)
{
    for (int i = 0; ksym_table[i].name != NULL; i++) {
        if (strcmp(ksym_table[i].name, name) == 0)
            return ksym_table[i].addr;
    }
    return 0;
}