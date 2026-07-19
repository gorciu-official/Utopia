# makefile adapted from https://github.com/Interpuce/AurorOS/blob/main/Makefile

ROOT_DIR  := .

BOOTLOADER     ?= limine
SUPPORTED_BOOTLOADERS := limine grub

ifeq ($(filter $(BOOTLOADER),$(SUPPORTED_BOOTLOADERS)),)
$(error Unsupported BOOTLOADER='$(BOOTLOADER)'. Supported values: $(SUPPORTED_BOOTLOADERS))
endif

SRC_DIR        := $(ROOT_DIR)/src
BIN_DIR        := $(ROOT_DIR)/bin/kernel.x86_64.$(BOOTLOADER)
ISO_DIR        := $(ROOT_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

KERNEL_BIN     := $(ROOT_DIR)/kernel.bin
ISO_FILE       := $(ROOT_DIR)/Utopia.iso
ifeq ($(BOOTLOADER),limine)
LINKER_SCRIPT  := $(SRC_DIR)/build/linker-limine.ld
endif
ifeq ($(BOOTLOADER),grub)
LINKER_SCRIPT  := $(SRC_DIR)/build/linker-grub.ld
endif
GRUB_CONFIG    := $(SRC_DIR)/build/grub.cfg
LIMINE_CONFIG  := $(SRC_DIR)/build/limine.conf

C_SOURCES      := $(shell find $(SRC_DIR) -type f -name '*.c' ! -name '*.excluded.c')
ifeq ($(BOOTLOADER),limine)
ASM_SOURCES    := $(shell find $(SRC_DIR) -type f -name '*.asm' ! -name 'boot.asm')
else 
ASM_SOURCES    := $(shell find $(SRC_DIR) -type f -name '*.asm')
endif

C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS    := $(patsubst $(SRC_DIR)/%.asm,$(BIN_DIR)/%.o,$(ASM_SOURCES))

OBJECTS        := $(C_OBJECTS) $(ASM_OBJECTS)

SMP_ENABLED    ?= true
USE_HOST_CPU   ?= true
SMP_CORES      ?= 4

LIMINE_DIR     := $(ROOT_DIR)/limine
LIMINE_URL     := https://github.com/limine-bootloader/limine.git
LIMINE_BRANCH  := v11.x-binary

EXTRA_QEMU_FLAGS ?=
QEMU_FLAGS := -cdrom $(ISO_FILE) -serial stdio -m 16G
ifeq ($(SMP_ENABLED),true)
QEMU_FLAGS += -smp $(SMP_CORES)
endif
ifeq ($(USE_HOST_CPU),true)
QEMU_FLAGS += -enable-kvm -cpu host,invtsc=on
endif
QEMU_FLAGS += $(EXTRA_QEMU_FLAGS)

all: build_kernel build_iso
	@echo -e "\033[32mSuccess!\033[0m"

ifeq ($(BOOTLOADER),limine)
$(LIMINE_DIR)/limine:
	@echo -e "\033[1;34m[*]\033[0m Downloading Limine..."
	@git clone $(LIMINE_URL) --branch=$(LIMINE_BRANCH) --depth=1 $(LIMINE_DIR)
	@echo -e "\033[1;34m[*]\033[0m Building Limine tool..."
	@$(MAKE) -C $(LIMINE_DIR) limine
endif

ifeq ($(BOOTLOADER),limine)
BOOTLOADER_VAL := 1
else
BOOTLOADER_VAL := 2
endif

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@gcc -O0 -g $(shell tr '\n' ' ' < compile_flags.txt) -DBOOTLOADER=$(BOOTLOADER_VAL) -c $< -o $@

$(BIN_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@nasm -f elf64 $< -o $@

build_archive:
	@mkdir -p $(ROOT_DIR)/initramfs/
	@tar --format=ustar -cf iso/initramfs.tar -C $(ROOT_DIR)/initramfs .

build_kernel: $(OBJECTS)
	@echo -e "\033[1;33m[*]\033[0m Linking objects -> kernel binary"
	@ld -m elf_x86_64 -T $(LINKER_SCRIPT) -o $(KERNEL_BIN) $(OBJECTS)

ifeq ($(BOOTLOADER),grub)
build_iso: build_kernel
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure"
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL_BIN) $(BOOT_DIR)
	@cp $(SRC_DIR)/build/grub.cfg $(GRUB_DIR)/grub.cfg
	@echo -e "\033[1;33m[*]\033[0m Generating ISO with GRUB"
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
endif

ifeq ($(BOOTLOADER),limine)
build_iso: build_kernel $(LIMINE_DIR)/limine
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure for Limine"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@make build_archive
	@cp $(KERNEL_BIN) $(ISO_DIR)/
	@cp $(LIMINE_CONFIG) $(ISO_DIR)/limine.conf
	@cp $(LIMINE_DIR)/limine-bios.sys $(ISO_DIR)/
	@cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_DIR)/
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_DIR)/
	@echo -e "\033[1;33m[*]\033[0m Generating ISO with xorriso"
	@xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_DIR) -o $(ISO_FILE) 2>/dev/null
	@echo -e "\033[1;33m[*]\033[0m Installing Limine boot record onto ISO"
	@$(LIMINE_DIR)/limine bios-install $(ISO_FILE)
endif

clean:
	@echo -e "\033[1;33m[*]\033[0m Cleaning..."
	@rm -rf $(BIN_DIR) $(ISO_DIR) $(KERNEL_BIN) $(ISO_FILE)

run: all
	qemu-system-x86_64 $(QEMU_FLAGS)

run_dbg: all
	@chmod +x scripts/run_debug_mode.sh 
	./scripts/run_debug_mode.sh $(QEMU_FLAGS)

recompile: clean all
