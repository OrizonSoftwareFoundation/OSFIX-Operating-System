/** 
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
*/

#include "../includes/acpi/acpi_fadt.h"
#include "../includes/acpi/acpi_shtdwn.h"
#include "includes/main/cpu/io.h"
static uint32_t g_pm1a_cnt_port = 0;
static uint32_t g_pm1b_cnt_port = 0;
static const uint8_t *g_dsdt_aml  = NULL;
static uint32_t       g_dsdt_len  = 0;

#define SLP_EN  (1u << 13)

acpi_status_t acpi_parse_fadt(void)
{
    acpi_sdt_header_t *hdr = NULL;
    acpi_status_t s = acpi_find_table("FACP", &hdr);
    if (s != ACPI_OK)
        return s;

    const acpi_fadt_t *fadt = (const acpi_fadt_t *)hdr;

    /*
     Prefer the 64-bit GAS fields (ACPI 2.0+, revision >= 2)
     These are more reliable on modern hardware; fall back to
     the legacy 32-bit port fields on ACPI 1.0 firmware.
     
     We only handle System I/O space (address_space == 1) here.
     Memory-mapped PM1 control blocks are uncommon on x86 and
     would require mmio accessors; Meaning
     I can always extend if the target needs it -Yazin T.
     */
    if (fadt->header.revision >= 2) {
        if (fadt->x_pm1a_cnt_blk.address_space == 1  &&
            fadt->x_pm1a_cnt_blk.address != 0)
        {
            g_pm1a_cnt_port = (uint32_t)fadt->x_pm1a_cnt_blk.address;
        } else {
            g_pm1a_cnt_port = fadt->pm1a_cnt_blk;
        }

        if (fadt->x_pm1b_cnt_blk.address_space == 1 &&
            fadt->x_pm1b_cnt_blk.address != 0)
        {
            g_pm1b_cnt_port = (uint32_t)fadt->x_pm1b_cnt_blk.address;
        } else {
            g_pm1b_cnt_port = fadt->pm1b_cnt_blk;
        }

        uintptr_t dsdt_addr = (fadt->x_dsdt != 0)
                              ? (uintptr_t)fadt->x_dsdt
                              : (uintptr_t)fadt->dsdt;
        acpi_sdt_header_t *dsdt_hdr =
            (acpi_sdt_header_t *)PHYS_TO_VIRT(dsdt_addr);
        g_dsdt_aml = (const uint8_t *)dsdt_hdr + sizeof(acpi_sdt_header_t);
        g_dsdt_len = dsdt_hdr->length - (uint32_t)sizeof(acpi_sdt_header_t);

    } else {
        g_pm1a_cnt_port = fadt->pm1a_cnt_blk;
        g_pm1b_cnt_port = fadt->pm1b_cnt_blk;

        acpi_sdt_header_t *dsdt_hdr =
            (acpi_sdt_header_t *)PHYS_TO_VIRT((uintptr_t)fadt->dsdt);
        g_dsdt_aml = (const uint8_t *)dsdt_hdr + sizeof(acpi_sdt_header_t);
        g_dsdt_len = dsdt_hdr->length - (uint32_t)sizeof(acpi_sdt_header_t);
    }

    return ACPI_OK;
}

#define AML_NAMEOP      0x08
#define AML_PACKAGEOP   0x12
#define AML_BYTEPREFIX  0x0A
#define AML_ZEROOP      0x00
#define AML_ONEOP       0x01

static int aml_pkg_length_size(uint8_t first_byte)
{
    return 1 + ((first_byte >> 6) & 0x3);
}

static acpi_status_t dsdt_find_s5(uint8_t *out_typa, uint8_t *out_typb)
{
    if (!g_dsdt_aml || g_dsdt_len < 8)
        return ACPI_ERR_NOT_FOUND;

    

    for (uint32_t i = 0; i + 8 < g_dsdt_len; i++) {
        if (g_dsdt_aml[i]   != '_' ||
            g_dsdt_aml[i+1] != 'S' ||
            g_dsdt_aml[i+2] != '5' ||
            g_dsdt_aml[i+3] != '_')
            continue;

        if (i == 0 || g_dsdt_aml[i - 1] != AML_NAMEOP)
            continue;

        uint32_t j = i + 4;
        if (g_dsdt_aml[j] != AML_PACKAGEOP)
            continue;
        j++;

        j += (uint32_t)aml_pkg_length_size(g_dsdt_aml[j]);

        j++;

        if (j + 4 > g_dsdt_len)
            continue;

        

        uint8_t typa = 0;
        if (g_dsdt_aml[j] == AML_BYTEPREFIX) {
            typa = g_dsdt_aml[j + 1];
            j += 2;
        } else if (g_dsdt_aml[j] == AML_ZEROOP) {
            typa = 0; j++;
        } else if (g_dsdt_aml[j] == AML_ONEOP) {
            typa = 1; j++;
        } else {
            continue;
        }

        uint8_t typb = 0;
        if (j < g_dsdt_len) {
            if (g_dsdt_aml[j] == AML_BYTEPREFIX && j + 1 < g_dsdt_len) {
                typb = g_dsdt_aml[j + 1];
            } else if (g_dsdt_aml[j] == AML_ONEOP) {
                typb = 1;
            }
           
        }

        *out_typa = typa;
        *out_typb = typb;
        return ACPI_OK;
    }

    return ACPI_ERR_NOT_FOUND;
}

acpi_status_t acpi_shutdown(void){
    if (g_pm1a_cnt_port == 0 || g_dsdt_aml == NULL)
        return ACPI_ERR_NOT_FOUND;  
    uint8_t slp_typa = 0, slp_typb = 0;
    acpi_status_t s = dsdt_find_s5(&slp_typa, &slp_typb);
    if (s != ACPI_OK)
        return s;

    

    uint16_t pm1a_val = (uint16_t)(((uint16_t)slp_typa << 10) | SLP_EN);
    uint16_t pm1b_val = (uint16_t)(((uint16_t)slp_typb << 10) | SLP_EN);

    outw((uint16_t)g_pm1a_cnt_port, pm1a_val);
    if (g_pm1b_cnt_port != 0)
        outw((uint16_t)g_pm1b_cnt_port, pm1b_val);

    return ACPI_ERR_NOT_FOUND;
}
