/**
 @copyright (C) 2026 Orizon Software Foundation
 @author Mejd A.
 @license: EUPL 1.2
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kprintf.h>
#include <util.h>
#include <stdlib.h>
#include <string.h>
#include <time/ktime.h>
#include "includes/hci/hci.h"
#include <log.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

volatile struct ehci_regs *ehci = NULL;
extern uint64_t pci_get_ehci_mmio(void);

static qh_t *async_list_head = NULL;

static uintptr_t async_list_head_phys = 0;

static uint32_t global_tag = 0x1000;

extern uint32_t pid_rn;

int ehci_stop_controller(void) {
    LOG_INFO("EHCI: stopping controller\n");
    ehci->USBCMD &= ~EHCI_RUN_STOP;

    uint32_t start = get_time_ms();
    while (!(ehci->USBSTS & EHCI_USBSTS_HCHALTED)) {
        if (get_time_ms() - start > 500) {
            LOG_INFO("EHCI: stop timeout, STS=%08x CMD=%08x, EHCIBASE=%08x\n", ehci->USBSTS, ehci->USBCMD, ehci);
            return -1;
        }
        udelay(1000);
    }

         LOG_INFO("EHCI: halted successfully, STS=%08x CMD=%08x\n", ehci->USBSTS, ehci->USBCMD);

    return 0;
}

int ehci_reset_controller(void) {
    LOG_INFO("EHCI: resetting controller\n");

    if (!(ehci->USBSTS & EHCI_USBSTS_HCHALTED)) {
        LOG_INFO("EHCI: controller not halted, stopping first\n");
        if (ehci_stop_controller() < 0) {
            LOG_INFO("EHCI: failed to stop before reset\n");
            return -2;
        }
    }

    udelay(1000);

    ehci->USBCMD = EHCI_RESET;
    LOG_INFO("EHCI: CMD after set reset=%08x\n", ehci->USBCMD);

    uint32_t start = get_time_ms();
    while (ehci->USBCMD & EHCI_RESET) {
        if (get_time_ms() - start > 1000) {
            LOG_INFO("EHCI: reset timeout, CMD=%08x STS=%08x, EHCIBASE=%08x\n",
                  ehci->USBCMD, ehci->USBSTS, ehci);
            return -1;
        }
        udelay(1000);
    }

    LOG_INFO("EHCI: reset complete, CMD=%08x STS=%08x\n",
          ehci->USBCMD, ehci->USBSTS);

    udelay(1000);
    return 0;
}

int ehci_init_async_list(void) {
    uint64_t qh_page_phys = palloc();
    if (!qh_page_phys) return -1;
    void *qh_page = phys_to_virt(qh_page_phys);
    
    memset(qh_page, 0, 4096);
    async_list_head = (qh_t *)qh_page;
    async_list_head_phys = (uintptr_t)virt_to_phys(qh_page);
    

    async_list_head->horiz_link = (uint32_t)async_list_head_phys | 0x2;
    async_list_head->ep_char = 0x00000000;
    async_list_head->ep_cap = 0x40000000;
    async_list_head->curr_qtd = 0x1;
    async_list_head->overlay.next = 0x1;
    async_list_head->overlay.token = 0x40;
    
    ehci->ASYNCLISTADDR = (uint32_t)async_list_head_phys;
    phys_flush_cache(async_list_head, sizeof(qh_t));
    
    return 0;
}

int ehci_init_simple(void) {
    LOG_INFO("EHCI init start\n");

    if (ehci_stop_controller() < 0) {
        LOG_INFO("EHCI stop failed\n");
        return -1;
    }

    LOG_INFO("EHCI stopped\n");

    if (ehci_reset_controller() < 0) {
        LOG_INFO("EHCI reset failed\n");
        return -2;
    }

    LOG_INFO("EHCI reset ok\n");

    if (ehci_init_async_list() < 0) {
        LOG_INFO("EHCI async list init failed\n");
        return -3;
    }

    LOG_INFO("Async list ok\n");

    ehci->PERIODICLISTBASE = 0;
    ehci->USBINTR = 0;
    ehci->USBCMD |= (EHCI_ASYNC_EN | EHCI_RUN_STOP);

    LOG_INFO("EHCI controller run command issued\n");

    uint32_t start = get_time_ms();
    while (!(ehci->USBSTS & (1 << 15))) {
        if (get_time_ms() - start > 100) {
            LOG_INFO("Timeout waiting for EHCI HCHalted bit clear\n");
            break;
        }
        udelay(100);
    }

    uint32_t cmd = ehci->USBCMD;
    uint32_t sts = ehci->USBSTS;
    LOG_INFO("EHCI USBCMD=0x%x, USBSTS=0x%x\n", cmd, sts);

    LOG_INFO("EHCI init done\n");
    return 0;
}

int ehci_port_connected(int port_num) {
    if (port_num < 0 || port_num >= 16) return 0;
    return (ehci->PORTSC[port_num] & PORTSC_CONNECTED) ? 1 : 0;
}

int ehci_reset_port(int port_num) {
    if (port_num < 0 || port_num >= 16) return -1;
    
    volatile uint32_t *portsc = &ehci->PORTSC[port_num];
    
    if (!(*portsc & PORTSC_CONNECTED)) return -2;
    
    if (*portsc & PORTSC_ENABLED) {
        *portsc &= ~PORTSC_ENABLED;
        udelay(1000);
    }
    
    *portsc |= PORTSC_RESET;
    udelay(50000);
    *portsc &= ~PORTSC_RESET;
    
    uint32_t start = get_time_ms();
    while (1) {
        if (*portsc & PORTSC_ENABLED) break;
        
        if (!(*portsc & PORTSC_RESET)) {
            *portsc |= PORTSC_OWNER;
            return -3;
        }
        
        if (get_time_ms() - start > 500) return -4;
        udelay(1000);
    }
    
    *portsc |= (PORTSC_CONNECT_CHANGE | PORTSC_ENABLE_CHANGE);
    udelay(10000);
    
    return 0;
}

void ehci_power_on_ports(int num_ports) {
    for (int i = 0; i < num_ports && i < 16; i++) {
        ehci->PORTSC[i] |= PORTSC_POWER;
    }
    udelay(20000);
}

int ehci_find_and_reset_device(int *port_num_out) {
    for (int i = 0; i < 16; i++) {
        if (ehci_port_connected(i)) {
            int rc = ehci_reset_port(i);
            if (rc == 0) {
                *port_num_out = i;
                return 0;
            }
        }
    }
    return -1;
}

int ehci_submit_bulk_simple(uint8_t devaddr, uint8_t ep_addr, void *data_vaddr, 
                           size_t len, int dir_in, uint32_t timeout_ms) {
    if (!async_list_head) return -1;
    
    uint64_t page_phys = palloc();
    if (!page_phys) return -1;
    void *page_alloced = phys_to_virt(page_phys);
    
    uintptr_t page_idx = 0;
    qh_t *qh = (qh_t *)(page_alloced + page_idx);
    page_idx += ALIGN_UP(sizeof(qh_t), 64);
    qtd_t *qtd = (qtd_t *)(page_alloced + page_idx);

    memset(qh, 0, sizeof(*qh));
    memset(qtd, 0, sizeof(*qtd));

    uintptr_t qh_phys = (uintptr_t)virt_to_phys(qh);
    uintptr_t qtd_phys = (uintptr_t)virt_to_phys(qtd);
    uintptr_t buf_phys = (uintptr_t)virt_to_phys(data_vaddr);
    (void)qtd_phys;

    qtd->next = 0x1;
    qtd->alt_next = 0x1;
    
    uint32_t pid = dir_in ? PID_IN : PID_OUT;
    qtd->token = (1u << 7) | (len << 16) | (3 << 10) | (pid << 8);
    qtd->buffer[0] = (uint32_t)buf_phys;
    
    for (size_t i = 1; i < 5 && (len > (i * 4096)); i++) {
        qtd->buffer[i] = (uint32_t)(buf_phys + (i * 4096)) & ~0xFFF;
    }

    uint8_t ep_num = ep_addr & 0x0F;
    uint16_t max_packet = 512;
    
    qh->ep_char = (devaddr << 0) | (ep_num << 8) | (max_packet << 16) | (2 << 12);
    qh->ep_cap = (1 << 30);
    qh->curr_qtd = 0;
    qh->overlay = *qtd;
    
    qh->horiz_link = async_list_head->horiz_link;
    phys_flush_cache(qh, sizeof(*qh));
    phys_flush_cache(qtd, sizeof(*qtd));
    phys_flush_cache(data_vaddr, len);
    
    async_list_head->horiz_link = (uint32_t)qh_phys | 0x2;
    phys_flush_cache(async_list_head, 64);
    
    uint32_t start = get_time_ms();
    while (1) {
        phys_invalidate_cache(qh, sizeof(*qh));
        uint32_t overlay_token = qh->overlay.token;
        
        if (!(overlay_token & (1u << 7))) break;
        
        if (overlay_token & 0x7E) {
            async_list_head->horiz_link = qh->horiz_link;
            phys_flush_cache(async_list_head, 64);
            pfree(page_phys);
            return -3;
        }
        
        if (get_time_ms() - start > timeout_ms) {
            async_list_head->horiz_link = qh->horiz_link;
            phys_flush_cache(async_list_head, 64);
            pfree(page_phys);
            return -2;
        }
        
        udelay(100);
    }
    
    async_list_head->horiz_link = qh->horiz_link;
    phys_flush_cache(async_list_head, 64);
    
    if (dir_in) {
        phys_invalidate_cache(data_vaddr, len);
    }
    
    pfree(page_phys);
    return 0;
}

#define EHCI_USBCMD    ((volatile uint32_t *)(ehci_base + 0x00))

#define EHCI_ASYNCLISTADDR ((volatile uint32_t *)(ehci_base + 0x18))

#define EHCI_USBSTS    ((volatile uint32_t *)(ehci_base + 0x04))

static int ehci_flush_async(void) {

    ehci->USBCMD &= ~(1 << 5);
    udelay(50);

    uint32_t timeout_ctr = 10000;

    while (ehci->USBSTS & (1 << 15) && timeout_ctr--)
        udelay(10);

    if (ehci->USBSTS & (1 << 15)) {
        return -1;
    }
    return 0;
}

static void ehci_run_async(void) {
    ehci->ASYNCLISTADDR = (uint32_t)(uintptr_t)async_list_head;
    udelay(20);
    ehci->USBCMD |= (1 << 5);
    udelay(20);
}

int usb_control_transfer(uint8_t devaddr, usb_ctrl_setup_t *setup,
                         void *data_buf, uint16_t data_len)
{
    if (!setup) {
        LOG_INFO("usb_control_transfer: setup ptr is NULL\n");
        return -1;
    }

    const size_t allocSize = 4096;
    uint64_t mem_phys = palloc();
    if (!mem_phys) {
        LOG_INFO("usb_control_transfer: palloc failed\n");
        return -2;
    }
    void *mem = phys_to_virt(mem_phys);
    memset(mem, 0, allocSize);

    uintptr_t phys_mem = (uintptr_t)virt_to_phys(mem);

    qh_t   *qh         = (qh_t *)          mem;
    qtd_t  *qtd_setup  = (qtd_t *)((uint8_t *)mem + 64);
    qtd_t  *qtd_data   = (qtd_t *)((uint8_t *)mem + 128);
    qtd_t  *qtd_status = (qtd_t *)((uint8_t *)mem + 192);

    memset(qh,         0, sizeof(qh_t));
    memset(qtd_setup,  0, sizeof(qtd_t));
    memset(qtd_data,   0, sizeof(qtd_t));
    memset(qtd_status, 0, sizeof(qtd_t));

    qtd_setup->next   = (uint32_t)(phys_mem + 128);
    qtd_setup->token  = (8 << 16);
    memcpy(qtd_setup->buffer, setup, sizeof(*setup));

    if (data_buf && data_len) {
        qtd_data->next  = (uint32_t)(phys_mem + 192);
        qtd_data->token = ((data_len & 0x7FFF) << 16);
        uintptr_t buf = (uintptr_t)data_buf;
        for (int i = 0; i < 5; i++) {
            qtd_data->buffer[i] = (uint32_t)buf;

            buf += 4096;
        }
    } else {

        qtd_setup->next = (uint32_t)(phys_mem + 192);
    }

    qtd_status->next  = 1;
    qtd_status->token = (0 << 16);

    qh->curr_qtd  = 0;
    qh->overlay   = *qtd_setup;
    qh->horiz_link = 1;
    qh->ep_char   = ((uint32_t)devaddr << 8) | (64 << 16);
    qh->ep_cap    = 0;

    phys_flush_cache(mem, allocSize);

    qh_t *old_head = async_list_head;
    async_list_head = qh;

    if (ehci_flush_async() != 0) {
        LOG_INFO("usb_control_transfer: ehci_flush_async failed\n");
        async_list_head = old_head;
        pfree(mem_phys);
        return -3;
    }
    ehci_run_async();

    uint64_t start = get_time_ms();
    while ((qh->overlay.token & (1 << 7))) {
        if ((get_time_ms() - start) > 5000) {
            LOG_INFO("usb_control_transfer: timeout\n");
            async_list_head = old_head;
            pfree(mem_phys);
            return -4;
        }
        udelay(100);
    }

    phys_invalidate_cache(mem, allocSize);

    int actual = data_len;
    if (data_len && !(qtd_data->token & 15<<3)) {
        actual = data_len - ((qtd_data->token >> 16) & 0x7FFF);
    }

    async_list_head = old_head;
    pfree(mem_phys);

    return actual;
}

int usb_get_device_descriptor(uint8_t devaddr, void *desc_buf, uint16_t len) {
    usb_ctrl_setup_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (0x01 << 8),
        .wIndex = 0,
        .wLength = len
    };
    return usb_control_transfer(devaddr, &setup, desc_buf, len);
}

int usb_get_config_descriptor(uint8_t devaddr, void *desc_buf, uint16_t len) {
    usb_ctrl_setup_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (0x02 << 8),
        .wIndex = 0,
        .wLength = len
    };
    return usb_control_transfer(devaddr, &setup, desc_buf, len);
}

int usb_set_address(uint8_t new_addr) {
    usb_ctrl_setup_t setup = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = new_addr,
        .wIndex = 0,
        .wLength = 0
    };
    int rc = usb_control_transfer(0, &setup, NULL, 0);
    if (rc == 0) {
        udelay(2000);
    }
    LOG_INFO("RCL: %u\n", rc);
    return rc;
}

int usb_set_configuration(uint8_t devaddr, uint8_t config_value) {
    usb_ctrl_setup_t setup = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = config_value,
        .wIndex = 0,
        .wLength = 0
    };
    return usb_control_transfer(devaddr, &setup, NULL, 0);
}

int usb_msd_reset(uint8_t devaddr, uint8_t iface_num) {
    usb_ctrl_setup_t setup = {
        .bmRequestType = 0x21,
        .bRequest = 0xFF,
        .wValue = 0,
        .wIndex = iface_num,
        .wLength = 0
    };
    return usb_control_transfer(devaddr, &setup, NULL, 0);
}

int find_mass_storage_device(usb_device_t *out_dev) {
    uint64_t desc_buf_phys = palloc();
    if (!desc_buf_phys) return -1;
    void *desc_buf = phys_to_virt(desc_buf_phys);
    
    LOG_INFO("1 \n");
    pid_rn=18;
    memset(desc_buf, 0, 4096);
    

    int res=usb_get_device_descriptor(0, desc_buf, 18);
    if (res != 0) {
        pfree(desc_buf_phys);
        return -100-res;
    }
    LOG_INFO("0x%x-%x-%x-%x", desc_buf);
    LOG_INFO("2 \n");
    pid_rn=19;
    

    if (usb_set_address(1) != 0) {
        pfree(desc_buf_phys);
        return -3;
    }
    LOG_INFO("3 \n");
    pid_rn=20;
    
    out_dev->address = 1;
    

    if (usb_get_config_descriptor(1, desc_buf, 9) != 0) {
        pfree(desc_buf_phys);
        return -4;
    }
    LOG_INFO("4 \n");
    pid_rn=21;
    
    usb_config_descriptor_t *cfg = (usb_config_descriptor_t *)desc_buf;
    uint16_t total_len = cfg->wTotalLength;
    
    if (total_len > 1024) total_len = 1024;
    

    if (usb_get_config_descriptor(1, desc_buf, total_len) != 0) {
        pfree(desc_buf_phys);
        return -5;
    }
    LOG_INFO("5 \n");
    pid_rn=22;
    
    out_dev->config_value = cfg->bConfigurationValue;
    

    uint8_t *ptr = (uint8_t *)desc_buf + 9;
    uint8_t *end = (uint8_t *)desc_buf + total_len;
    int found_msd = 0;

    LOG_INFO("Start 0x%x, end 0x%x", ptr, end);
    
    while (ptr < end) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];

        if (len < 2) {
            LOG_INFO("Bad descriptor length %u at %p\n", len, ptr);
            break;
        }

        if (type == 0x04 && len >= sizeof(usb_interface_descriptor_t)) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)ptr;
            LOG_INFO("Interface #%u: class=%02x subclass=%02x proto=%02x\n",
                            iface->bInterfaceNumber,
                            iface->bInterfaceClass,
                            iface->bInterfaceSubClass,
                            iface->bInterfaceProtocol);

            if (iface->bInterfaceClass == USB_CLASS_MASS_STORAGE &&
                iface->bInterfaceSubClass == USB_SUBCLASS_SCSI &&
                iface->bInterfaceProtocol == USB_PROTOCOL_BULK_ONLY) {
                out_dev->iface_number = iface->bInterfaceNumber;
                found_msd = 1;
                LOG_INFO("  -> Found Mass Storage interface\n");
            }

        } else if (type == 0x05 && len >= sizeof(usb_endpoint_descriptor_t)) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)ptr;
            LOG_INFO("  Endpoint addr=%02x attr=%02x maxpkt=%u\n",
                            ep->bEndpointAddress,
                            ep->bmAttributes,
                            ep->wMaxPacketSize);

            if (found_msd && (ep->bmAttributes & 0x03) == 0x02) {
                if (ep->bEndpointAddress & 0x80) {
                    out_dev->bulk_in.addr = ep->bEndpointAddress;
                    out_dev->bulk_in.max_packet = ep->wMaxPacketSize;
                    out_dev->bulk_in.attrs = ep->bmAttributes;
                    LOG_INFO("    -> Bulk IN endpoint registered\n");
                } else {
                    out_dev->bulk_out.addr = ep->bEndpointAddress;
                    out_dev->bulk_out.max_packet = ep->wMaxPacketSize;
                    out_dev->bulk_out.attrs = ep->bmAttributes;
                    LOG_INFO("    -> Bulk OUT endpoint registered\n");
                }
            }

        } else {
            LOG_INFO("Other descriptor type=%02x len=%u\n", type, len);
        }

        ptr += len;
    }

    pfree(desc_buf_phys);

    if (!found_msd) {
        LOG_INFO("No Mass Storage interface found.\n");
        return -6;
    }

    

    if (usb_set_configuration(out_dev->address, out_dev->config_value) != 0) {
        return -7;
    }
    
    udelay(100000);
    

    usb_msd_reset(out_dev->address, out_dev->iface_number);
    udelay(100000);
    
    return 0;
}

int msd_send_cbw(usb_device_t *dev, void *cbw_buf) {
    return ehci_submit_bulk_simple(dev->address, dev->bulk_out.addr, 
                                   cbw_buf, 31, 0, 1000);
}

int msd_receive_csw(usb_device_t *dev, void *csw_buf) {
    return ehci_submit_bulk_simple(dev->address, dev->bulk_in.addr, 
                                   csw_buf, 13, 1, 1000);
}

int msd_read_sector(usb_device_t *dev, uint32_t lba, void *buf_vaddr) {
    uint64_t page_phys = palloc();
    if (!page_phys) return -1;
    void *page_alloced = phys_to_virt(page_phys);
    
    uintptr_t page_sharing_index = 0;
    struct CBW *cbw = (struct CBW *)(page_alloced + page_sharing_index);
    page_sharing_index += ALIGN_UP(sizeof(struct CBW), 16);
    struct CSW *csw = (struct CSW *)(page_alloced + page_sharing_index);

    memset(cbw, 0, sizeof(*cbw));
    cbw->signature = CBW_SIGNATURE;
    cbw->tag = ++global_tag;
    cbw->data_transfer_length = 512;
    cbw->flags = 0x80;
    cbw->lun = 0;
    cbw->cb_length = 10;
    cbw->cb[0] = SCSI_READ10;
    cbw->cb[1] = 0;
    cbw->cb[2] = (lba >> 24) & 0xFF;
    cbw->cb[3] = (lba >> 16) & 0xFF;
    cbw->cb[4] = (lba >> 8) & 0xFF;
    cbw->cb[5] = (lba >> 0) & 0xFF;
    cbw->cb[7] = 0;
    cbw->cb[8] = 1;

    phys_flush_cache(cbw, sizeof(*cbw));
    if (msd_send_cbw(dev, cbw) != 0) {
        pfree(page_phys);
        return -2;
    }

    if (ehci_submit_bulk_simple(dev->address, dev->bulk_in.addr, 
                               buf_vaddr, 512, 1, 1000) != 0) {
        pfree(page_phys);
        return -3;
    }

    memset(csw, 0, sizeof(*csw));
    if (msd_receive_csw(dev, csw) != 0) {
        pfree(page_phys);
        return -4;
    }

    phys_invalidate_cache(csw, sizeof(*csw));
    if (csw->signature != CSW_SIGNATURE) {
        pfree(page_phys);
        return -5;
    }
    if (csw->tag != cbw->tag) {
        pfree(page_phys);
        return -6;
    }
    if (csw->status != 0) {
        pfree(page_phys);
        return -7;
    }

    pfree(page_phys);
    return 0;
}

int read_sector0_example(void) {
    uint16_t ports;
    uintptr_t mmio_base;
    void *bar0_virt = NULL;
    
    void *mmio = pci_map_ehci_mmio(&ports, &mmio_base, bar0_virt);
    if (!mmio) return -1;
    ehci = (volatile struct ehci_regs *)mmio;

    if (ehci_init_simple() != 0) return -2;
    
    ehci_power_on_ports(ports);
    
    int port_num = -1;
    if (ehci_find_and_reset_device(&port_num) != 0) return -3;

    usb_device_t dev;
    if (find_mass_storage_device(&dev) != 0) return -4;

    uint64_t sector_buf_phys = palloc();
    if (!sector_buf_phys) return -5;
    void *sector_buf = phys_to_virt(sector_buf_phys);
    memset(sector_buf, 0, 512);

    int rc = msd_read_sector(&dev, 0, sector_buf);
    if (rc != 0) {
        pfree(sector_buf_phys);
        return rc - 5;
    }

    pfree(sector_buf_phys);
    return 0;
}

void *pci_map_ehci_mmio(uint16_t *num_ports, uintptr_t *mmio_base_out, void *bar0_virt) {    
    if (!bar0_virt) return NULL;
    

    volatile ehci_cap_regs_t *cap_regs = (volatile ehci_cap_regs_t *)bar0_virt;
    uint8_t caplength = cap_regs->CAPLENGTH;
    

    uint32_t hcsparams = cap_regs->HCSPARAMS;
    uint16_t n_ports = HCSPARAMS_N_PORTS(hcsparams);
    

    int has_ppc = HCSPARAMS_PPC(hcsparams);
    (void)has_ppc;
    

    void *op_regs = (void *)((uintptr_t)bar0_virt + caplength);
    

    if (num_ports) *num_ports = n_ports;
    if (mmio_base_out) *mmio_base_out = (uintptr_t)bar0_virt;

    ehci = bar0_virt;
    
    return op_regs;
}

void ehci_print_capabilities(void *bar0_virt) {
    volatile ehci_cap_regs_t *cap = (volatile ehci_cap_regs_t *)bar0_virt;
    
    uint8_t caplength = cap->CAPLENGTH;
    uint16_t hciversion = cap->HCIVERSION;
    uint32_t hcsparams = cap->HCSPARAMS;
    uint32_t hccparams = cap->HCCPARAMS;
    
    LOG_INFO("EHCI Capability Registers:\n");
    LOG_INFO("  CAPLENGTH:  0x%x (%d bytes)\n", caplength, caplength);
    LOG_INFO("  HCIVERSION: 0x%x (USB %d.%d)\n", 
                   hciversion, (hciversion >> 8) & 0xFF, hciversion & 0xFF);
    LOG_INFO("  HCSPARAMS:  0x%x\n", hcsparams);
    LOG_INFO("    N_PORTS:  %d\n", HCSPARAMS_N_PORTS(hcsparams));
    LOG_INFO("    PPC:      %s\n", HCSPARAMS_PPC(hcsparams) ? "Yes" : "No");
    LOG_INFO("    N_CC:     %d companion controllers\n", HCSPARAMS_N_CC(hcsparams));
    LOG_INFO("  HCCPARAMS:  0x%x\n", hccparams);
    LOG_INFO("  Op Regs at: BAR0 + 0x%x\n", caplength);
    
}

void ehci_init(uint64_t phys_mmio) {
    volatile ehci_cap_regs_t *cap =
        (volatile ehci_cap_regs_t *)phys_to_virt(phys_mmio);

    volatile struct ehci_regs *op =
        (volatile struct ehci_regs *)((uintptr_t)cap + cap->CAPLENGTH);

    LOG_INFO("EHCI version %x\n", cap->HCIVERSION);

    op->USBCMD &= ~1;
    while (!(op->USBSTS & (1 << 12)));

    op->USBCMD |= (1 << 1);
    while (op->USBCMD & (1 << 1));

    uint64_t qh_phys = palloc();
    void *qh_virt = phys_to_virt(qh_phys);
    memset(qh_virt, 0, 4096);

    uint32_t *qh = qh_virt;
    qh[0] = qh_phys | 0x2;
    qh[1] = (1 << 30);
    qh[2] = 1;

    op->ASYNCLISTADDR = qh_phys;

    op->USBCMD |= (1 << 5);

    op->USBCMD |= 1;

    op->CONFIGFLAG = 1;

    LOG_INFO("EHCI controller started\n");
}

int module_init(void) {
    uint64_t phys = pci_get_ehci_mmio();
    if (!phys) { LOG_INFO("No EHCI MMIO\n"); return 0; }
    ehci_init(phys);
    return 0;
}