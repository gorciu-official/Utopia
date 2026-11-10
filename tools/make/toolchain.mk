CC             := clang
LD             := ld.lld
RUSTC          ?= rustc

ifeq ($(ARCH),x86_64)
    AS := nasm
    RUST_TARGET := x86_64-unknown-none
else ifeq ($(ARCH),riscv64)
    AS := $(CC)
    RUST_TARGET := riscv64gc-unknown-none-elf
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

CFLAGS         ?= 
CFLAGS         += $(shell tr '\n' ' ' < tools/compile_flags.txt) -DUTOPIA_COMMIT=\"$(shell git rev-parse --short HEAD)\"
RUSTFLAGS      ?=
RUSTFLAGS      += $(shell cat $(TARGET_DIR)/../rust-flags.txt)
ASFLAGS        :=
LDFLAGS        := -T $(LINKER_SCRIPT)

ifeq ($(ARCH),x86_64)
    CFLAGS += -m64 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -DARCHITECTURE=1
    ASFLAGS += -f elf64
    LDFLAGS += -m elf_x86_64 
endif
ifeq ($(ARCH),riscv64)
    CFLAGS += -march=rv64gc_zihintpause -mabi=lp64d -DARCHITECTURE=2
    ASFLAGS += -march=rv64gc_zihintpause -c
    LDFLAGS += -m elf64lriscv 
endif

C_TARGET ?=

ifneq ($(C_TARGET),)
CFLAGS += --target=$(C_TARGET)
ASFLAGS += --target=$(C_TARGET)
endif
