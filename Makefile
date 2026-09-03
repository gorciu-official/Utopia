# makefile adapted from https://github.com/Interpuce/AurorOS/blob/main/Makefile
# TODO: rewrite to nob.c

ROOT_DIR  := .

BOOTLOADER            ?= limine
SUPPORTED_BOOTLOADERS := limine grub

ARCH                    ?= x86_64
SUPPORTED_ARCHITECTURES := x86_64 riscv64

ifeq ($(filter $(BOOTLOADER),$(SUPPORTED_BOOTLOADERS)),)
$(error Unsupported BOOTLOADER='$(BOOTLOADER)'. Supported values: $(SUPPORTED_BOOTLOADERS))
endif

ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHITECTURES)),)
$(error Unsupported ARCH='$(ARCH)'. Supported values: $(SUPPORTED_ARCHITECTURES))
endif

ifeq ($(ARCH),riscv64)
    $(warning RISC-V 64 support in Utopia is extremelly experimental and WILL break)
endif 

SRC_DIR        := $(ROOT_DIR)/src

# binary dirs
TARGET_DIR     := $(ROOT_DIR)/target/kernel.$(ARCH).$(BOOTLOADER)
OBJ_DIR        := $(TARGET_DIR)/obj
ISO_DIR        := $(TARGET_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

# final outputs
KERNEL_BIN     := $(TARGET_DIR)/Utopia.bin
ISO_FILE       := $(TARGET_DIR)/Utopia.iso

# linker script
LINKER_SCRIPT  := $(SRC_DIR)/build/linker-$(ARCH)-$(BOOTLOADER).ld

# bootloader config
GRUB_CONFIG    := $(SRC_DIR)/build/grub.cfg
LIMINE_CONFIG  := $(SRC_DIR)/build/limine.conf

CC             ?= cc 
AS             ?= nasm
LD             ?= ld

ifeq ($(AS),nasm)
    ifneq ($(ARCH),x86_64)
        $(error NASM is great, but incompatibile with other than x86_64 architectures supported by Utopia. Please choose a different assembler)
    endif
else 
    ifeq ($(ARCH),x86_64)
        ifeq (,$(findstring nasm,$(AS)))
            ifeq ($(shell command -v nasm 2>/dev/null),)
                $(error Compiling Utopia for x86_64 architecture requires NASM assembler. No command nasm exists and nasm is not the AS)
            else
                AS := nasm
            endif
        endif
    endif
endif

CFLAGS         := $(shell tr '\n' ' ' < compile_flags.txt)
ASFLAGS        :=
LDFLAGS        := -T $(LINKER_SCRIPT)

ifeq ($(ARCH),x86_64)
    CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -DARCHITECTURE=1
    ASFLAGS += -f elf64
    LDFLAGS += -m elf_x86_64 
endif
ifeq ($(ARCH),riscv64)
    CFLAGS += -march=rv64gc_zihintpause -mabi=lp64d -DARCHITECTURE=2
    ASFLAGS += -march=rv64gc_zihintpause
    LDFLAGS += -m elf64lriscv 
endif

C_SOURCES := $(shell find $(SRC_DIR) -type f -name '*.c' ! -name '*.excluded.c' ! -path '$(SRC_DIR)/arch/*')
ifeq ($(BOOTLOADER),limine)
ASM_SOURCES := $(shell find $(SRC_DIR) -type f \( -name '*.asm' -o -name '*.S' \) ! -path '$(SRC_DIR)/arch/*')
else
ASM_SOURCES := $(shell find $(SRC_DIR) -type f \( -name '*.asm' -o -name '*.S' \) ! -path '$(SRC_DIR)/arch/*')
endif

C_SOURCES   += $(shell find $(SRC_DIR)/arch/$(ARCH) -type f -name '*.c')
ifeq ($(BOOTLOADER),limine)
ASM_SOURCES += $(shell find $(SRC_DIR)/arch/$(ARCH) -type f \( -name '*.asm' -o -name '*.S' \) ! -name 'grub-preinit.asm')
else
ASM_SOURCES += $(shell find $(SRC_DIR)/arch/$(ARCH) -type f \( -name '*.asm' -o -name '*.S' \))
endif

C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS := \
	$(patsubst $(SRC_DIR)/%.asm,$(OBJ_DIR)/%.o,$(filter %.asm,$(ASM_SOURCES))) \
	$(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(filter %.S,$(ASM_SOURCES)))

OBJECTS        := $(C_OBJECTS) $(ASM_OBJECTS)

SMP_ENABLED    ?= true
USE_HOST_CPU   ?= true
SMP_CORES      ?= 4

LIMINE_DIR     := $(ROOT_DIR)/limine
LIMINE_URL     := https://github.com/limine-bootloader/limine.git
LIMINE_BRANCH  := v11.x-binary

ifeq ($(ARCH),riscv64)
QEMU := qemu-system-riscv64
endif
ifeq ($(ARCH),x86_64)
QEMU := qemu-system-x86_64
endif

EXTRA_QEMU_FLAGS ?=
QEMU_FLAGS := -cdrom $(ISO_FILE) -serial stdio -m 1G
ifeq ($(SMP_ENABLED),true)
QEMU_FLAGS += -smp $(SMP_CORES)
endif
ifeq ($(USE_HOST_CPU),true)
QEMU_FLAGS += -enable-kvm -cpu host,invtsc=on
endif
ifeq ($(ARCH),riscv64)
QEMU_FLAGS += -machine virt 
QEMU_FLAGS += -drive if=pflash,format=raw,unit=0,file=target/third-party.riscv64.edk2/RISCV_VIRT_CODE.fd,readonly=on
QEMU_FLAGS += -drive if=pflash,format=raw,unit=1,file=target/third-party.riscv64.edk2/RISCV_VIRT_VARS.fd 
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

build_deps:
	@chmod +x scripts/mk_bootloader_cfg.sh && ./scripts/mk_bootloader_cfg.sh -e
	@if [ -d initramfs ]; then tar --format=ustar -cf $(ISO_DIR)/initramfs.tar -C $(ROOT_DIR)/initramfs . ; fi
	@if [ -f font.psf1 ]; then cp font.psf1 $(ISO_DIR)/ ; fi

build_kernel: $(OBJECTS)
	@echo -e "\033[1;33m[*]\033[0m Linking objects -> kernel binary"
	@$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJECTS)

ifeq ($(BOOTLOADER),grub)
build_iso: build_kernel
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(GRUB_DIR)
	@make build_deps
	@cp $(KERNEL_BIN) $(BOOT_DIR)/kernel.bin
	@cp $(SRC_DIR)/build/grub.cfg $(GRUB_DIR)/grub.cfg
	@echo -e "\033[1;33m[*]\033[0m Generating ISO with GRUB"
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
endif

ifeq ($(BOOTLOADER),limine)
ifeq ($(ARCH),x86_64)
build_iso: build_kernel $(LIMINE_DIR)/limine
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure for Limine"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@make build_deps
	@cp $(KERNEL_BIN) $(ISO_DIR)/kernel.bin
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
ifeq ($(ARCH),riscv64)
build_iso: build_kernel $(LIMINE_DIR)/limine
	@echo -e "\033[1;33m[*]\033[0m Creating RISC-V64 Limine ISO directory"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@make build_deps
	@cp $(KERNEL_BIN) $(ISO_DIR)/kernel.bin
	@cp $(LIMINE_CONFIG) $(ISO_DIR)/limine.conf
	@echo -e "\033[1;33m[*]\033[0m Creating FAT EFI System Partition"
	@rm -f $(ISO_DIR)/efiboot.img
	@truncate -s 16M $(ISO_DIR)/efiboot.img
	@mkfs.fat -F 16 -n UTOPIA_EFI $(ISO_DIR)/efiboot.img
	@mmd -i $(ISO_DIR)/efiboot.img ::/EFI
	@mmd -i $(ISO_DIR)/efiboot.img ::/EFI/BOOT
	@mcopy -i $(ISO_DIR)/efiboot.img \
		$(LIMINE_DIR)/BOOTRISCV64.EFI \
		::/EFI/BOOT/BOOTRISCV64.EFI
	@echo -e "\033[1;33m[*]\033[0m Generating RISC-V64 ISO"
	@xorriso -as mkisofs \
		-R \
		-J \
		-V "MYOS" \
		-eltorito-alt-boot \
		-e efiboot.img \
		-no-emul-boot \
		$(ISO_DIR) \
		-o $(ISO_FILE)
endif
endif

clean:
	@echo -e "\033[1;33m[*]\033[0m Cleaning..."
	@rm -rf $(TARGET_DIR)

run: all
	$(QEMU) $(QEMU_FLAGS)

run_dbg: all
	@chmod +x scripts/run_debug_mode.sh 
	./scripts/run_debug_mode.sh $(KERNEL_BIN) $(QEMU_FLAGS)

recompile: clean all
