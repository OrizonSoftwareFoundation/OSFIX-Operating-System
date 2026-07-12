/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Yazin T.
 @license: EUPL 1.2
 */

#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stddef.h>

#define ACPI_PACKED __attribute__((packed))

#ifndef PHYS_TO_VIRT
#   define PHYS_TO_VIRT(addr)  ((void *)(uintptr_t)(addr))
#endif

typedef enum {
    ACPI_OK              =  0,
    ACPI_ERR_NOT_FOUND   = -1,
    ACPI_ERR_CHECKSUM    = -2,
    ACPI_ERR_NO_TABLE    = -3,
    ACPI_ERR_NULL        = -4,
} acpi_status_t;

typedef struct ACPI_PACKED {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} acpi_rsdp_t;

typedef struct ACPI_PACKED {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct ACPI_PACKED {
    acpi_sdt_header_t header;
    uint32_t          tables[];
} acpi_rsdt_t;

typedef struct ACPI_PACKED {
    acpi_sdt_header_t header;
    uint64_t          tables[];
} acpi_xsdt_t;

acpi_status_t      acpi_init(void);
acpi_status_t      acpi_find_table(const char sig[4], acpi_sdt_header_t **out);
const acpi_rsdp_t *acpi_get_rsdp(void);
acpi_status_t acpi_init_from_addr(void *rsdp_addr);
#endif