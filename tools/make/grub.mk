ifeq ($(BOOTLOADER),grub)

# iso build target

iso: kernel
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure"
	@rm -rf $(ISO_DIR)
	@mkdir -p $(GRUB_DIR)
	@make bootmodules
	@cp $(KERNEL_BIN) $(BOOT_DIR)/kernel.bin
	@cp $(GRUB_CONFIG) $(GRUB_DIR)/grub.cfg
	@echo -e "\033[1;33m[*]\033[0m Generating ISO with GRUB"
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
endif
