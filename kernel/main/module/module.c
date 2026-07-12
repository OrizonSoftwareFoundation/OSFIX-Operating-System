/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "includes/main/module/module.h"
#include <kprintf.h>
#include <stdlib.h>
#include <string.h>
#include <cpu/io.h>
#include <time/ktime.h>
#include <log.h>

static void _log_ok  (const char *m) { LOG_OK  ("%s", m); }
static void _log_info(const char *m) { LOG_INFO("%s", m); }
static void _log_warn(const char *m) { LOG_WARN("%s", m); }
static void _log_fatal (const char *m) { LOG_FATAL ("%s", m); }

const kernel_api_t kernel_api = {
    .kprintf  = kprintf,

    .malloc   = malloc,
    .calloc   = calloc,
    .realloc  = realloc,
    .free     = free,

    .memcpy   = memcpy,
    .memset   = memset,
    .memcmp   = memcmp,
    .strlen   = strlen,
    .strcpy   = strcpy,
    .strncpy  = strncpy,
    .strcmp   = strcmp,
    .strncmp  = strncmp,
    .strstr   = strstr,
    .strchr   = strchr,

    .outb     = outb,
    .outw     = outw,
    .outl     = outl,
    .inb      = inb,
    .inw      = inw,
    .inl      = inl,

    .log_ok   = _log_ok,
    .log_info = _log_info,
    .log_warn = _log_warn,
    .log_fatal  = _log_fatal,
};

