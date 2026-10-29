# makefile adapted from https://github.com/Interpuce/AurorOS/blob/main/Makefile

# mock target to satisfy make bc gnu make treats the first target found
# as the default one.
default_target: all

ROOT_DIR  := .

ARCH                    ?= x86_64
SUPPORTED_ARCHITECTURES := x86_64 riscv64

DEBUG                   ?= false

ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHITECTURES)),)
    $(error Unsupported ARCH='$(ARCH)'. Supported values: $(SUPPORTED_ARCHITECTURES))
endif

SRC_DIR        := $(ROOT_DIR)/src

# bootloader configuration
include scripts/make/bootloaders.mk

# binary dirs

ifeq ($(DEBUG),true)
    TARGET_DIR     := $(ROOT_DIR)/target/kernel.debug.$(ARCH).$(BOOTLOADER)
else
    TARGET_DIR     := $(ROOT_DIR)/target/kernel.release.$(ARCH).$(BOOTLOADER)
endif

OBJ_DIR        := $(TARGET_DIR)/obj
ISO_DIR        := $(TARGET_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

# final outputs
KERNEL_BIN     := $(TARGET_DIR)/Utopia.bin
ISO_FILE       := $(TARGET_DIR)/Utopia.iso

# linker script
LINKER_SCRIPT  := $(SRC_DIR)/build/linker-$(ARCH)-$(BOOTLOADER).ld

# toolchain 
include scripts/make/toolchain.mk 

# sources and object files
include scripts/make/src-obj.mk 

# common build rules

all: hooks kernel iso
	@echo -e "\033[32mSuccess!\033[0m"

kernel: $(OBJECTS)
	@echo -e "\033[1;33m[*]\033[0m Linking objects -> kernel binary"
	@$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJECTS)

bootmodules:
	@chmod +x scripts/mk_bootloader_cfg.sh && ./scripts/mk_bootloader_cfg.sh -e
	@if [ -d initramfs-$(ARCH) ]; then tar --format=ustar -cf $(ISO_DIR)/initramfs.tar -C $(ROOT_DIR)/initramfs-$(ARCH) . ; fi
	@if [ -f font.psf1 ]; then cp font.psf1 $(ISO_DIR)/ ; fi

# extension-specific

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@$(CC) -O0 -g $(CFLAGS) -DBOOTLOADER=$(BOOTLOADER_VAL) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@$(AS) $(ASFLAGS) -g $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@$(AS) $(ASFLAGS) -g $< -o $@

# misc

clean:
	@rm -rf $(TARGET_DIR)
	@echo -e "\033[1;33m[*]\033[0m Cleaned up. (architecture = $(ARCH), bootloader = $(BOOTLOADER))"

recompile: clean all

hooks:
	@chmod +x scripts/git/*
	@git config core.hooksPath scripts/git

# qemu commands
include scripts/make/qemu.mk

# header tracking if present
-include $(C_OBJECTS:.o=.d)
