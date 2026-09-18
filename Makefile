BOOTLOADER            ?= limine
SUPPORTED_BOOTLOADERS := limine grub

ARCH                    ?= x86_64
SUPPORTED_ARCHITECTURES := x86_64 riscv64

ROOT_DIR       := .
KERNEL_SRC_DIR := $(ROOT_DIR)/kernel/src

TARGET_DIR     := $(ROOT_DIR)/target/kernel.$(ARCH).$(BOOTLOADER)
OBJ_DIR        := $(TARGET_DIR)/obj
ISO_DIR        := $(TARGET_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

KERNEL_BIN     := $(TARGET_DIR)/Utopia.bin
ISO_FILE       := $(TARGET_DIR)/Utopia.iso

GRUB_CONFIG    := $(KERNEL_SRC_DIR)/build/grub.cfg
LIMINE_CONFIG  := $(KERNEL_SRC_DIR)/build/limine.conf

LIMINE_DIR     := $(ROOT_DIR)/target/third-party.all.limine-binaries
LIMINE_URL     := https://github.com/limine-bootloader/limine.git
LIMINE_BRANCH  := v11.x-binary

ifeq ($(filter $(BOOTLOADER),$(SUPPORTED_BOOTLOADERS)),)
$(error Unsupported BOOTLOADER='$(BOOTLOADER)'. Supported values: $(SUPPORTED_BOOTLOADERS))
endif

ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHITECTURES)),)
$(error Unsupported ARCH='$(ARCH)'. Supported values: $(SUPPORTED_ARCHITECTURES))
endif

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
QEMU_FLAGS += -machine virt -device ramfb
QEMU_FLAGS += -drive if=pflash,format=raw,unit=0,file=target/third-party.riscv64.edk2/RISCV_VIRT_CODE.fd,readonly=on
QEMU_FLAGS += -drive if=pflash,format=raw,unit=1,file=target/third-party.riscv64.edk2/RISCV_VIRT_VARS.fd 
endif
QEMU_FLAGS += $(EXTRA_QEMU_FLAGS)

all: build_kernel build_iso
	@echo -e "\033[92;1mCompilation success!\033[0m"

build_kernel:
	make -C kernel

build_deps:
	@chmod +x scripts/mk_bootloader_cfg.sh && ./scripts/mk_bootloader_cfg.sh -e
	@if [ -d initramfs ]; then tar --format=ustar -cf $(ISO_DIR)/initramfs.tar -C $(ROOT_DIR)/initramfs . ; fi
	@if [ -f font.psf1 ]; then cp font.psf1 $(ISO_DIR)/ ; fi

ifeq ($(BOOTLOADER),limine)
$(LIMINE_DIR)/limine:
	@echo -e "\033[1;34m[*]\033[0m Downloading Limine..."
	@mkdir -p $(LIMINE_DIR)
	@git clone $(LIMINE_URL) --branch=$(LIMINE_BRANCH) --depth=1 $(LIMINE_DIR)
	@echo -e "\033[1;34m[*]\033[0m Building Limine tool..."
	@$(MAKE) CC=cc AS=as LD=ld -C $(LIMINE_DIR) limine
endif

ifeq ($(BOOTLOADER),grub)
build_iso: build_kernel
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(GRUB_DIR)
	@make build_deps
	@cp $(KERNEL_BIN) $(BOOT_DIR)/kernel.bin
	@cp $(KERNEL_SRC_DIR)/build/grub.cfg $(GRUB_DIR)/grub.cfg
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
	./scripts/run_debug_mode.sh $(QEMU) $(KERNEL_BIN) $(QEMU_FLAGS)

recompile: clean all
