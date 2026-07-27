#ifndef DRIVERS_SATA_H
#define DRIVERS_SATA_H

#include <stdint.h>

#define SATA_WAIT_TIMEOUT 1000

#define SATA_WAIT_TIMEOUT_PER_SECTOR 250

typedef struct {
    char type_identifier;   

    void *device_data;      

    size_t sector_size;     

} legacy_storage_device_t;

#define MAX_STORAGE_DEVICES 1028 

extern legacy_storage_device_t storage_devices[MAX_STORAGE_DEVICES];

extern uint32_t storage_device_count;

typedef struct {
    uint8_t cl[1024];       

    uint8_t rfis[256];      

    uint8_t ct[4096 * 32];  

} ahci_port_mem_t;

typedef struct {
    uint8_t fis_type;       

    uint8_t pmport : 4;     

    uint8_t rsv0 : 3;       

    uint8_t c : 1;          

    uint8_t command;        

    uint8_t featurel;       

    uint8_t lba0;           

    uint8_t lba1;           

    uint8_t lba2;           

    uint8_t device;         

    uint8_t lba3;           

    uint8_t lba4;           

    uint8_t lba5;           

    uint8_t featureh;       

    uint8_t countl;         

    uint8_t counth;         

    uint8_t icc;            

    uint8_t control;        

    uint8_t rsv1[4];        

} fis_reg_h2d_t;

typedef struct {
    uint16_t flags;         

    uint16_t prdt_length;   

    uint32_t prdbc;         

    uint32_t ctba;          

    uint32_t ctbau;         

    uint32_t rsv1[4];       

} hba_cmd_header_t;

typedef struct {
    uint32_t dba;           

    uint32_t dbau;          

    uint32_t rsv0;          

    uint32_t dbc : 22;      

    uint32_t rsv1 : 9;      

    uint32_t i : 1;         

} hba_prdt_entry_t;

typedef struct {
    uint8_t cfis[64];               

    uint8_t acmd[16];               

    uint8_t rsv[48];                

    hba_prdt_entry_t prdt_entry[128]; 

} hba_cmd_tbl_t;

int sata_ahci_read_sector(volatile uint32_t *port_base, uint64_t lba, uint16_t sector_count, void *buffer);

int sata_ahci_write_sector(volatile uint32_t *port_base, uint64_t lba, uint16_t sector_count, const void *buffer);

int sata_ahci_identify(volatile uint32_t *port_base, void *buffer);

void sata_print_sector_size(uint16_t *identify_data);

int sata_ahci_read_sector_satapi(volatile uint32_t *port_base, uint32_t lba, uint16_t sector_count, void *buffer);

int sata_ahci_identify_satapi(volatile uint32_t *port_base, void *buffer);

int sata_ahci_identify_satapi_properly(volatile uint32_t *port_base, void *buffer);

void sata_search(uintptr_t mmio_base);

void sata_init(void);

volatile uint32_t *sata_get_port(void);

int sata_read_lba(uint64_t lba, uint16_t count, void *buffer);

int sata_write_lba(uint64_t lba, uint16_t count, const void *buffer);
#endif
