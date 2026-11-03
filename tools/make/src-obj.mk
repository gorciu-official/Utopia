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

# using one object file for entire rust source files fixes multiple issues that
# were occuring before, so that's what i'll use
RUST_SOURCES := $(SRC_DIR)/rust/main.rs

C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))
RUST_OBJECTS   := $(patsubst $(SRC_DIR)/%.rs,$(OBJ_DIR)/%.o,$(RUST_SOURCES))
ASM_OBJECTS := \
	$(patsubst $(SRC_DIR)/%.asm,$(OBJ_DIR)/%.o,$(filter %.asm,$(ASM_SOURCES))) \
	$(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(filter %.S,$(ASM_SOURCES)))

OBJECTS        := $(C_OBJECTS) $(ASM_OBJECTS) $(RUST_OBJECTS)
