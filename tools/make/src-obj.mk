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

RUST_SOURCES := $(shell find $(SRC_DIR)/rust -type f -name '*.rs')

C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
RUST_ARCHIVE   := $(TARGET_DIR)/rust/$(RUST_TARGET)/release/librust_for_utopia.a
RUST_OBJECTS   := $(RUST_ARCHIVE)
ASM_OBJECTS := \
	$(patsubst $(SRC_DIR)/%.asm,$(OBJ_DIR)/%.o,$(filter %.asm,$(ASM_SOURCES))) \
	$(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(filter %.S,$(ASM_SOURCES)))

OBJECTS        := $(C_OBJECTS) $(ASM_OBJECTS) $(RUST_OBJECTS)
