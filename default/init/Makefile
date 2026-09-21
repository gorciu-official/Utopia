ARCH                    ?= x86_64
SUPPORTED_ARCHITECTURES := x86_64 riscv64

ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHITECTURES)),)
$(error Unsupported ARCH='$(ARCH)'. Supported values: $(SUPPORTED_ARCHITECTURES))
endif

ROOT_DIR       := $(abspath $(CURDIR)/../..)

SRC_DIR        := $(ROOT_DIR)/default/init/src
TARGET_DIR     := $(ROOT_DIR)/target/default.init.$(ARCH)
OBJ_DIR        := $(TARGET_DIR)/obj
TARGET_FILE    := $(TARGET_DIR)/init

CROSS_COMPILE  ?= 
CC             := $(CROSS_COMPILE)gcc 
CFLAGS         := $(shell tr '\n' ' ' < $(ROOT_DIR)/default/init/compile_flags.txt)

C_SOURCES      := $(shell find $(SRC_DIR) -type f -name '*.c')
C_OBJECTS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SOURCES))

all: $(C_OBJECTS)
	@echo -e "\033[1;36m[*]\033[0m sources -> binary"
	@gcc $(CFLAGS) $(C_OBJECTS) -o $(TARGET_FILE)
	@echo -e "\033[32mDefault init process built successfully!\033[0m"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo -e "\033[1;36m[*]\033[0m $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@
