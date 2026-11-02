ifeq ($(BOOTLOADER),limine)

LIMINE_DIR     := $(ROOT_DIR)/target/third-party.all.limine-binaries
LIMINE_URL     := https://github.com/limine-bootloader/limine.git
LIMINE_BRANCH  := v11.x-binary

# Limine requires us to download its binary release, so let's do that
$(LIMINE_DIR)/limine:
	@echo -e "\033[1;34m[*]\033[0m Downloading Limine..."
	@mkdir -p $(LIMINE_DIR)
	@git clone $(LIMINE_URL) --branch=$(LIMINE_BRANCH) --depth=1 $(LIMINE_DIR)
	@echo -e "\033[1;34m[*]\033[0m Building Limine tool..."
	@$(MAKE) CC=cc AS=as LD=ld -C $(LIMINE_DIR) limine

# ISO build targets

ifeq ($(ARCH),x86_64)
iso: kernel $(LIMINE_DIR)/limine
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure for Limine"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)
	@make bootmodules
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
iso: kernel $(LIMINE_DIR)/limine
	@echo -e "\033[1;33m[*]\033[0m Creating RISC-V64 Limine ISO directory"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@make bootmodules
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
		-V "Utopia" \
		-eltorito-alt-boot \
		-e efiboot.img \
		-no-emul-boot \
		$(ISO_DIR) \
		-o $(ISO_FILE)
endif

endif
