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
