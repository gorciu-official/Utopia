# makefile adapted from https://github.com/Interpuce/AurorOS/blob/main/Makefile

ROOT_DIR  := ..

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

SRC_DIR        := $(ROOT_DIR)/kernel/src

# binary dirs
TARGET_DIR     := $(ROOT_DIR)/target/kernel.$(ARCH).$(BOOTLOADER)
OBJ_DIR        := $(TARGET_DIR)/obj
ISO_DIR        := $(TARGET_DIR)/iso
BOOT_DIR       := $(ISO_DIR)/boot
GRUB_DIR       := $(BOOT_DIR)/grub

# final outputs
KERNEL_BIN     := $(TARGET_DIR)/Utopia.bin

# linker script
LINKER_SCRIPT  := $(SRC_DIR)/build/linker-$(ARCH)-$(BOOTLOADER).ld

CROSS_COMPILE  ?= 
CC             := $(CROSS_COMPILE)gcc 
LD             := $(CROSS_COMPILE)ld

ifeq ($(ARCH),x86_64)
    AS := nasm
else ifeq ($(ARCH),riscv64)
    AS := $(CROSS_COMPILE)as
endif

ifeq ($(ARCH),x86_64)
    ifeq (,$(findstring nasm,$(AS)))
        ifeq ($(shell command -v nasm 2>/dev/null),)
            $(error Compiling Utopia for x86_64 architecture requires NASM assembler. No command nasm exists and nasm is not the AS)
        else
            AS := nasm
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

all: build_kernel
	@echo -e "\033[32mKernel built successfully!\033[0m"

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

build_kernel: $(OBJECTS)
	@echo -e "\033[1;33m[*]\033[0m Linking objects -> kernel binary"
	@$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJECTS)
