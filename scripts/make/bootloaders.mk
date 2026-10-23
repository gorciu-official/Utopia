BOOTLOADER            ?= limine
SUPPORTED_BOOTLOADERS := limine grub

ifeq ($(filter $(BOOTLOADER),$(SUPPORTED_BOOTLOADERS)),)
   $(error Unsupported BOOTLOADER='$(BOOTLOADER)'. Supported values: $(SUPPORTED_BOOTLOADERS))
endif

# make expects us to write shit relative to the root Makefile
include scripts/make/grub.mk
include scripts/make/limine.mk

ifeq ($(BOOTLOADER),limine)
BOOTLOADER_VAL := 1
else
BOOTLOADER_VAL := 2
endif

# bootloader config
GRUB_CONFIG    := $(SRC_DIR)/build/grub.cfg
LIMINE_CONFIG  := $(SRC_DIR)/build/limine.conf
