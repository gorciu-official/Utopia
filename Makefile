# makefile adapted from https://github.com/Interpuce/AurorOS/blob/main/Makefile

ROOT_DIR  := .

SRC_DIR        := $(ROOT_DIR)/src
BIN_DIR        := $(ROOT_DIR)/bin
ISO_DIR        := $(ROOT_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

KERNEL_BIN     := $(ROOT_DIR)/kernel.bin
ISO_FILE       := $(ROOT_DIR)/AurorOS.iso
LINKER_SCRIPT  := $(SRC_DIR)/build/linker.ld
GRUB_CONFIG    := $(SRC_DIR)/build/grub.cfg

C_SOURCES      := $(shell find $(SRC_DIR) -type f -name '*.c' ! -name '*.excluded.c')
ASM_SOURCES    := $(shell find $(SRC_DIR) -type f -name '*.asm')

C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%.o,$(C_SOURCES))
ASM_OBJECTS    := $(patsubst $(SRC_DIR)/%.asm,$(BIN_DIR)/%.o,$(ASM_SOURCES))

OBJECTS        := $(C_OBJECTS) $(ASM_OBJECTS)

all: build_kernel build_iso
	@echo -e "\033[32mSuccess!\033[0m"

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@gcc -g -Wall -Wextra -m64 -mno-sse -mno-sse2 -mno-sse3 -mno-mmx -ffreestanding -nostartfiles -Iinclude -nostdlib -fno-stack-protector -c $< -o $@

$(BIN_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@nasm -f elf64 $< -o $@

build_kernel: $(OBJECTS)
	@echo -e "\033[1;33m[*]\033[0m Linking objects -> kernel binary"
	@ld -m elf_x86_64 -T $(LINKER_SCRIPT) -o $(KERNEL_BIN) $(OBJECTS)

build_iso: build_kernel
	@echo -e "\033[1;33m[*]\033[0m Creating ISO directory structure"
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL_BIN) $(BOOT_DIR)
	@cp $(SRC_DIR)/build/grub.cfg $(GRUB_DIR)/grub.cfg
	@echo -e "\033[1;33m[*]\033[0m Generating ISO with GRUB"
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

clean:
	@echo -e "\033[1;33m[*]\033[0m Cleaning..."
	@rm -rf $(BIN_DIR) $(ISO_DIR) $(KERNEL_BIN) $(ISO_FILE)

run: all
	qemu-system-x86_64 -cdrom AurorOS.iso

run_dbg: all
	@chmod +x scripts/run_debug_mode.sh
	./scripts/run_debug_mode.sh 

recompile: clean all
