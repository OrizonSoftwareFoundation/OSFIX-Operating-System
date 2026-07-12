#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define EHCI_RUN_STOP     (1u << 0)
#define EHCI_RESET        (1u << 1)
#define EHCI_PERIODIC_EN  (1u << 4)
#define EHCI_ASYNC_EN     (1u << 5)

#define EHCI_USBSTS_HCHALTED (1u << 12)

#define USB_DIR_IN  0x80
#define USB_DIR_OUT 0x00

#define USB_REQ_GET_DESCRIPTOR  0x06
#define USB_REQ_SET_ADDRESS     0x05
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_CLASS_MASS_STORAGE 0x08
#define USB_SUBCLASS_SCSI      0x06
#define USB_PROTOCOL_BULK_ONLY 0x50

#define PORTSC_CONNECTED        (1 << 0)
#define PORTSC_CONNECT_CHANGE   (1 << 1)
#define PORTSC_ENABLED          (1 << 2)
#define PORTSC_ENABLE_CHANGE    (1 << 3)
#define PORTSC_RESET            (1 << 8)
#define PORTSC_POWER            (1 << 12)
#define PORTSC_OWNER            (1 << 13)

#define PID_OUT   0
#define PID_IN    1
#define PID_SETUP 2

typedef struct {
    uint8_t  CAPLENGTH;      
    uint8_t  reserved;
    uint16_t HCIVERSION;     
    uint32_t HCSPARAMS;      
    uint32_t HCCPARAMS;      
    uint64_t HCSP_PORTROUTE; 
} __attribute__((packed)) ehci_cap_regs_t;

#define HCSPARAMS_N_PORTS(x)     ((x) & 0x0F)           
#define HCSPARAMS_PPC(x)         (((x) >> 4) & 0x01)    
#define HCSPARAMS_N_PCC(x)       (((x) >> 8) & 0x0F)    
#define HCSPARAMS_N_CC(x)        (((x) >> 12) & 0x0F)   

struct __attribute__((packed)) CBW {
    uint32_t signature; 
    uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t  flags;
    uint8_t  lun;
    uint8_t  cb_length;
    uint8_t  cb[16];
};

struct __attribute__((packed)) CSW {
    uint32_t signature; 
    uint32_t tag;
    uint32_t data_residue;
    uint8_t  status;
};

#define CBW_SIGNATURE 0x43425355u
#define CSW_SIGNATURE 0x53425355u

#define SCSI_READ10  0x28
#define SCSI_INQUIRY 0x12

struct ehci_regs {
    volatile uint32_t USBCMD;
    volatile uint32_t USBSTS;
    volatile uint32_t USBINTR;
    volatile uint32_t FRINDEX;
    volatile uint32_t CTRLDSSEGMENT;
    volatile uint32_t PERIODICLISTBASE;
    volatile uint32_t ASYNCLISTADDR;
    volatile uint32_t reserved1[9];
    volatile uint32_t CONFIGFLAG;
    volatile uint32_t PORTSC[16];
};

extern volatile struct ehci_regs *ehci;

typedef struct {
    uint32_t next;
    uint32_t alt_next;
    uint32_t token;
    uint32_t buffer[5];
} __attribute__((packed, aligned(32))) qtd_t;

typedef struct {
    uint32_t horiz_link;
    uint32_t ep_char;
    uint32_t ep_cap;
    uint32_t curr_qtd;
    uint32_t reserved[4];
    qtd_t overlay;
} __attribute__((packed, aligned(64))) qh_t;

typedef struct {
    uint8_t  addr;
    uint16_t max_packet;
    uint8_t  attrs;
} usb_endpoint_t;

typedef struct {
    uint8_t  address;
    uint8_t  config_value;
    usb_endpoint_t bulk_in;
    usb_endpoint_t bulk_out;
    uint8_t  iface_number;
} usb_device_t;

typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_ctrl_setup_t;

typedef struct {
    usb_device_t *dev;
    uint32_t pipe;
    usb_ctrl_setup_t *setup;
    void *data;
    int length;
    int actual_length;
    void *context;
    void (*complete)(void *);
} usb_transfer_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

void *pci_map_ehci_mmio(uint16_t *num_ports, uintptr_t *mmio_base_out, void *bar0_virt);
void ehci_print_capabilities(void *bar0_virt);

int ehci_stop_controller(void);
int ehci_reset_controller(void);
int ehci_init_async_list(void);
int ehci_init_simple(void);

int ehci_port_connected(int port_num);
int ehci_reset_port(int port_num);
void ehci_power_on_ports(int num_ports);

int ehci_find_and_reset_device(int *port_num_out);
int ehci_submit_bulk_simple(uint8_t devaddr, uint8_t ep_addr, void *data_vaddr, 
                           size_t len, int dir_in, uint32_t timeout_ms);

int usb_control_transfer(uint8_t devaddr, usb_ctrl_setup_t *setup, 
                        void *data, uint16_t data_len);
int usb_get_device_descriptor(uint8_t devaddr, void *desc_buf, uint16_t len);
int usb_get_config_descriptor(uint8_t devaddr, void *desc_buf, uint16_t len);
int usb_set_address(uint8_t new_addr);
int usb_set_configuration(uint8_t devaddr, uint8_t config_value);
int usb_msd_reset(uint8_t devaddr, uint8_t iface_num);

int find_mass_storage_device(usb_device_t *out_dev);
int msd_send_cbw(usb_device_t *dev, void *cbw_buf);
int msd_receive_csw(usb_device_t *dev, void *csw_buf);
int msd_read_sector(usb_device_t *dev, uint32_t lba, void *buf_vaddr);
void ehci_init(uint64_t phys_mmio);
void ehci_pci_init(uint8_t bus, uint8_t dev, uint8_t func);
void init_ehci_usb(uint8_t bus, uint8_t device, uint8_t func);