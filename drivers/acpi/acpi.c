/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#include "../includes/acpi/acpi.h"
#include <log.h>

static const acpi_rsdp_t     *g_rsdp  = NULL;
static const acpi_rsdt_t     *g_rsdt  = NULL;
static const acpi_xsdt_t     *g_xsdt  = NULL;

static uint8_t acpi_checksum(const void *base, size_t len)
{
    const uint8_t *ptr = (const uint8_t *)base;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += ptr[i];
    return sum;
}

static const acpi_rsdp_t *try_rsdp(const void *ptr)
{
    const acpi_rsdp_t *r = (const acpi_rsdp_t *)ptr;

    const char sig[8] = { 'R','S','D',' ','P','T','R',' ' };
    for (int i = 0; i < 8; i++)
        if (r->signature[i] != sig[i])
            return NULL;

    if (acpi_checksum(r, 20) != 0)
        return NULL;

    if (r->revision >= 2 && acpi_checksum(r, r->length) != 0)
        return NULL;

    return r;
}

static const acpi_rsdp_t *rsdp_search_range(uintptr_t start, uintptr_t end)
{
    for (uintptr_t addr = start; addr < end; addr += 16) {
        const acpi_rsdp_t *r = try_rsdp(PHYS_TO_VIRT(addr));
        if (r) return r;
    }
    return NULL;
}

static const acpi_rsdp_t *rsdp_find(void)
{
    uint16_t ebda_seg = *(const uint16_t *)PHYS_TO_VIRT(0x040E);
    uintptr_t ebda_base = (uintptr_t)ebda_seg << 4;
    if (ebda_base >= 0x80000 && ebda_base < 0xA0000) {
        const acpi_rsdp_t *r = rsdp_search_range(ebda_base, ebda_base + 1024);
        if (r) return r;
    }
    return rsdp_search_range(0xE0000, 0x100000);
}

acpi_status_t acpi_init(void)
{
    g_rsdp = rsdp_find();
    if (!g_rsdp) {
        LOG_FATAL("ACPI: RSDP not found\n");
        SERIAL(Fatal, acpi_init, "ACPI: RSDP not found\n");
        return ACPI_ERR_NOT_FOUND;
    }

    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_address != 0) {
        g_xsdt = (const acpi_xsdt_t *)PHYS_TO_VIRT(g_rsdp->xsdt_address);
        if (acpi_checksum(g_xsdt, g_xsdt->header.length) != 0) {
            g_xsdt = NULL;
            LOG_FATAL("ACPI: XSDT checksum failed\n");
            SERIAL(Fatal, acpi_init, "ACPI: XSDT checksum failed\n");
            return ACPI_ERR_CHECKSUM;
        }
        LOG_OK("ACPI: XSDT found at %x\n", g_rsdp->xsdt_address);
        SERIAL(Ok, acpi_init, "ACPI: XSDT found at %x\n", g_rsdp->xsdt_address);
    } else {
        g_rsdt = (const acpi_rsdt_t *)PHYS_TO_VIRT(g_rsdp->rsdt_address);
        if (acpi_checksum(g_rsdt, g_rsdt->header.length) != 0) {
            g_rsdt = NULL;
            LOG_FATAL("ACPI: RSDT checksum failed\n");
            SERIAL(Fatal, acpi_init, "ACPI: RSDT checksum failed\n");
            return ACPI_ERR_CHECKSUM;
        }
        LOG_OK("ACPI: RSDT found at %x\n", g_rsdp->rsdt_address);
        SERIAL(Ok, acpi_init, "ACPI: RSDT found at %x\n", g_rsdp->rsdt_address);
    }

    LOG_OK("ACPI: initialized via scan (revision %d)\n", g_rsdp->revision);
    SERIAL(Ok, acpi_init, "ACPI: initialized via scan (revision %d)\n", g_rsdp->revision);
    return ACPI_OK;
}

acpi_status_t acpi_init_from_addr(void *rsdp_addr)
{
    if (!rsdp_addr) {
        LOG_FATAL("ACPI: null RSDP address passed\n");
        SERIAL(Fatal, acpi_init_from_addr, "ACPI: null RSDP address passed\n");
        return ACPI_ERR_NULL;
    }

    g_rsdp = (const acpi_rsdp_t *)rsdp_addr;

    if (acpi_checksum(g_rsdp, 20) != 0) {
        g_rsdp = NULL;
        LOG_FATAL("ACPI: RSDP checksum failed\n");
        SERIAL(Fatal, acpi_init_from_addr, "ACPI: RSDP checksum failed\n");
        return ACPI_ERR_CHECKSUM;
    }

    if (g_rsdp->revision >= 2 && g_rsdp->xsdt_address != 0) {
        g_xsdt = (const acpi_xsdt_t *)PHYS_TO_VIRT(g_rsdp->xsdt_address);
        if (acpi_checksum(g_xsdt, g_xsdt->header.length) != 0) {
            g_xsdt = NULL;
            LOG_FATAL("ACPI: XSDT checksum failed\n");
            SERIAL(Fatal, acpi_init_from_addr, "ACPI: XSDT checksum failed\n");
            return ACPI_ERR_CHECKSUM;
        }
        LOG_OK("ACPI: XSDT found at %x\n", g_rsdp->xsdt_address);
        SERIAL(Ok, acpi_init_from_addr, "ACPI: XSDT found at %x\n", g_rsdp->xsdt_address);
    } else {
        g_rsdt = (const acpi_rsdt_t *)PHYS_TO_VIRT(g_rsdp->rsdt_address);
        if (acpi_checksum(g_rsdt, g_rsdt->header.length) != 0) {
            g_rsdt = NULL;
            LOG_FATAL("ACPI: RSDT checksum failed\n");
            SERIAL(Fatal, acpi_init_from_addr, "ACPI: RSDT checksum failed\n");
            return ACPI_ERR_CHECKSUM;
        }
        LOG_OK("ACPI: RSDT found at %x\n", g_rsdp->rsdt_address);
        SERIAL(Ok, acpi_init_from_addr, "ACPI: RSDT found at %x\n", g_rsdp->rsdt_address);
    }

    LOG_OK("ACPI: initialized from Limine RSDP (revision %d)\n", g_rsdp->revision);
    SERIAL(Ok, acpi_init_from_addr, "ACPI: initialized from Limine RSDP (revision %d)\n", g_rsdp->revision);
    return ACPI_OK;
}

acpi_status_t acpi_find_table(const char sig[4], acpi_sdt_header_t **out)
{
    if (!out) {
        SERIAL(Fatal, acpi_find_table, "ACPI: null out pointer\n");
        return ACPI_ERR_NULL;
    }
    if (!g_rsdp) {
        LOG_FATAL("ACPI: acpi_find_table called before init\n");
        SERIAL(Fatal, acpi_find_table, "ACPI: acpi_find_table called before init\n");
        return ACPI_ERR_NOT_FOUND;
    }

    if (g_xsdt) {
        size_t entry_count = (g_xsdt->header.length - sizeof(acpi_sdt_header_t))
                             / sizeof(uint64_t);

        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *hdr =
                (acpi_sdt_header_t *)PHYS_TO_VIRT(g_xsdt->tables[i]);

            if (hdr->signature[0] == sig[0] &&
                hdr->signature[1] == sig[1] &&
                hdr->signature[2] == sig[2] &&
                hdr->signature[3] == sig[3])
            {
                if (acpi_checksum(hdr, hdr->length) != 0) {
                    SERIAL(Fatal, acpi_find_table, "ACPI: table checksum failed\n");
                    return ACPI_ERR_CHECKSUM;
                }
                *out = hdr;
                return ACPI_OK;
            }
        }
        LOG_WARN("ACPI: table '%.4s' not found in XSDT\n", sig);
        SERIAL(Warn, acpi_find_table, "ACPI: table not found in XSDT\n");
        return ACPI_ERR_NO_TABLE;
    }

    if (g_rsdt) {
        size_t entry_count = (g_rsdt->header.length - sizeof(acpi_sdt_header_t))
                             / sizeof(uint32_t);

        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *hdr =
                (acpi_sdt_header_t *)PHYS_TO_VIRT(g_rsdt->tables[i]);

            if (hdr->signature[0] == sig[0] &&
                hdr->signature[1] == sig[1] &&
                hdr->signature[2] == sig[2] &&
                hdr->signature[3] == sig[3])
            {
                if (acpi_checksum(hdr, hdr->length) != 0) {
                    SERIAL(Fatal, acpi_find_table, "ACPI: table checksum failed\n");
                    return ACPI_ERR_CHECKSUM;
                }
                *out = hdr;
                return ACPI_OK;
            }
        }
        LOG_WARN("ACPI: table '%.4s' not found in RSDT\n", sig);
        SERIAL(Warn, acpi_find_table, "ACPI: table not found in RSDT\n");
        return ACPI_ERR_NO_TABLE;
    }

    return ACPI_ERR_NOT_FOUND;
}

const acpi_rsdp_t *acpi_get_rsdp(void)
{
    return g_rsdp;
}

