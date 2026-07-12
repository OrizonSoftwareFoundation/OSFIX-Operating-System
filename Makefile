CC      := cc
LD      := $(CC)
LD_BIN  := ld
NASM    := nasm
NASMFLAGS := -f elf64

KERNEL    := build/kernel.elf
IMAGE     := OSFIX.img
BUILD_DIR := build
INITRAMFS := $(BUILD_DIR)/initramfs.cpio

#Fuck Arch, Fuck Debian, Fuck Ubuntu especially, Fuck everything. -Y.T.
OVMF_CANDIDATES := \
	/usr/share/edk2/x64/OVMF.4m.fd \
	/usr/share/edk2-ovmf/x64/OVMF.4m.fd \
	/usr/share/OVMF/OVMF.fd \
	/usr/share/ovmf/OVMF.fd \
	/usr/share/qemu/OVMF.fd

QEMU_FIRMWARE := $(firstword $(wildcard $(OVMF_CANDIDATES)))

ifndef QEMU_FIRMWARE
$(error Could not find OVMF firmware. Install edk2-ovmf or ovmf package.)
endif

CFLAGS  := -Iboot \
           -Ikernel \
           -Ikernel/main \
           -Ikernel/includes/main/util \
           -Idrivers \
           -Idrivers/includes \
           -Idrivers/includes/storage \
           -Idrivers/includes/pci \
           -Idrivers/includes/hci \
           -I. \
           -Ikernel/includes/main/mm/heapalloc \
           -Ikernel/includes/main \
           -Ikernel/includes/main/util/print \
           -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-stack-check -fno-lto -fno-pie -fno-pic \
           -m64 -march=x86-64 -mno-80387 -mno-mmx \
           -mno-red-zone -mcmodel=kernel

# Drivers are loaded at arbitrary addresses by the module loader so they
# cannot use PC-relative 32-bit relocations (-mcmodel=kernel).
# -mcmodel=large forces absolute 64-bit relocations instead. - The big T.
DRIVER_CFLAGS := $(filter-out -mcmodel=kernel,$(CFLAGS)) \
                 -mcmodel=large -mno-red-zone -fno-pic -fno-pie \
                 -mno-sse -mno-sse2

LDFLAGS := -nostdlib -static -Wl,-m,elf_x86_64 \
           -z max-page-size=0x1000 -T linker.ld


#  Fuck you. Add a new module by appending one line here.
#  Format: name:source_dir
#  The module will be built as build/initramfs/<name>/<name>.elf
MODULES := \
	apic:drivers/pic/apic  \
	storage:drivers/storage \
	pci:drivers/pci         \
	hci:drivers/hci         \
	exfat:fs/exfat


# Kernel sources; everything except module dirs and drivers/
# (drivers/acpi is the one kernel-side exception because fuck you)
MODULE_DIRS   := $(foreach m,$(MODULES),$(word 2,$(subst :, ,$(m))))
EXCLUDE_PATHS := $(foreach d,$(MODULE_DIRS),! -path "./$(d)/*") ! -path "./drivers/*"

C_SOURCES   := $(shell find . -name "*.c" $(EXCLUDE_PATHS)) \
               $(wildcard drivers/acpi/*.c)
ASM_SOURCES := $(shell find . -name "*.asm" $(EXCLUDE_PATHS))

OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
       $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

define MODULE_template
$(1)_SRCS := $$(wildcard $(2)/*.c)
$(1)_OBJS := $$(patsubst $(2)/%.c,$(BUILD_DIR)/$(2)/%.o,$$($(1)_SRCS))
$(1)_ELF  := $(BUILD_DIR)/initramfs/$(1)/$(1).elf

ifneq ($$($(1)_OBJS),)
DRIVER_ELFS += $$($(1)_ELF)
endif

$$($(1)_ELF): $$($(1)_OBJS)
	@echo "  LD (mod) $$@"
	@mkdir -p $$(dir $$@)
	@$(LD_BIN) -r -m elf_x86_64 -o $$@ $$^

$(BUILD_DIR)/$(2)/%.o: $(2)/%.c
	@echo "  CC     $$<"
	@mkdir -p $$(dir $$@)
	@$(CC) $(DRIVER_CFLAGS) -c $$< -o $$@
endef

DRIVER_ELFS :=
$(foreach m,$(MODULES),$(eval $(call MODULE_template,$(word 1,$(subst :, ,$(m))),$(word 2,$(subst :, ,$(m))))))

.PHONY: all clean image run run-amd docs
.SECONDARY: $(foreach m,$(MODULES),$($(word 1,$(subst :, ,$(m)))_OBJS))

all: clean $(KERNEL) $(INITRAMFS)

$(KERNEL): $(OBJ)
	@mkdir -p $(BUILD_DIR)
	@echo "  LD     $@"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@ -lgcc

$(INITRAMFS): $(DRIVER_ELFS)
	@echo "  CPIO   $@"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR)/initramfs && \
	    find . -type f -name "*.elf" | sort | \
	    cpio --quiet -o --format=newc > $(abspath $(INITRAMFS))

$(BUILD_DIR)/%.o: %.c
	@echo "  CC     $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	@echo "  NASM   $<"
	@mkdir -p $(dir $@)
	@$(NASM) $(NASMFLAGS) $< -o $@

image: $(KERNEL) $(INITRAMFS)
	@echo "Creating UEFI Disk Image... (may require sudo)"
	@dd if=/dev/zero of=$(IMAGE) bs=1M count=64 > /dev/null 2>&1
	@sudo parted -s $(IMAGE) mklabel gpt
	@sudo parted -s $(IMAGE) mkpart ESP fat32 2048s 100%
	@sudo parted -s $(IMAGE) set 1 esp on

	@mformat -i $(IMAGE)@@1M ::
	@mmd -i $(IMAGE)@@1M ::/boot
	@mmd -i $(IMAGE)@@1M ::/EFI
	@mmd -i $(IMAGE)@@1M ::/EFI/BOOT

	@mcopy -i $(IMAGE)@@1M $(KERNEL) ::/boot/
	@mcopy -i $(IMAGE)@@1M $(INITRAMFS) ::/boot/
	@mcopy -i $(IMAGE)@@1M boot/assets/limine.conf ::/boot/
	@mcopy -i $(IMAGE)@@1M boot/BOOTX64.EFI ::/EFI/BOOT/

run-amd: image
	@qemu-system-x86_64 \
		-machine q35 \
		-cpu EPYC-Rome \
		-m 4G \
		-serial stdio \
		-bios $(QEMU_FIRMWARE) \
		-drive file=$(IMAGE),format=raw \
		-display sdl

run: image
	@qemu-system-x86_64 \
		-machine q35 \
		-m 1G \
		-serial stdio \
		-bios $(QEMU_FIRMWARE) \
		-drive file=$(IMAGE),format=raw \
		-device usb-ehci,id=ehci \
		-device usb-host,vendorid=0x046d,productid=0xc534 \
		-display sdl

#This does NOT need a comment
clean:
	@echo "  CLEAN"
	@rm -rf $(KERNEL) $(BUILD_DIR) $(IMAGE)
	@rm -rf Documentation/html
	@rm -rf Documentation/latex

