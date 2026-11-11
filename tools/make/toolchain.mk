CC             := clang
LD             := ld.lld
RUSTC          ?= rustc

ifeq ($(shell command -v $(CC) 2>/dev/null),)
	$(error Utopia requires $(CC) as a C compiler, please install it.)
endif

ifeq ($(shell command -v $(LD) 2>/dev/null),)
    $(error Utopia requires $(LD) as a linker, please install it.)
endif

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
CFLAGS         += $(shell tr '\n' ' ' < compile_flags.txt) -g -DUTOPIA_COMMIT=\"$(shell git rev-parse --short HEAD)\"
RUSTFLAGS      ?=
RUSTFLAGS      += $(shell cat $(TARGET_DIR)/../rust-flags.txt) $(C_RS_FLAGS)
ASFLAGS        ?=
ASFLAGS        += -g
LDFLAGS        := -T $(LINKER_SCRIPT)

LTO            ?=

ifeq ($(LTO),thin)
    CFLAGS += -flto=thin
    RUSTFLAGS += -C embed-bitcode=yes -C lto=thin
else ifeq ($(LTO),full)
    CFLAGS += -flto=full
    RUSTFLAGS += -C embed-bitcode=yes -C lto=fat
else ifneq ($(LTO),)
    $(error Unsupported LTO='$(LTO)'. Use thin or full)
endif

OPTLVL         ?=

ifneq ($(OPTLVL),)
    ifneq ($(filter $(OPTLVL),0 1 2 3 s z),$(OPTLVL))
        $(error Unsupported OPTLVL='$(OPTLVL)'. Use 0, 1, 2, 3, s, or z)
    endif
    CFLAGS += -O$(OPTLVL)
    RUSTFLAGS += -C opt-level=$(OPTLVL)
endif

ifeq ($(ARCH),x86_64)
    CFLAGS += -m64 -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -DARCHITECTURE=1
    ASFLAGS += -f elf64
    LDFLAGS += -m elf_x86_64 
endif
ifeq ($(ARCH),riscv64)
    CFLAGS += -march=rv64gc_zihintpause -mabi=lp64d -DARCHITECTURE=2
    ASFLAGS += -march=rv64gc_zihintpause -c
    LDFLAGS += -m elf64lriscv 
endif

ifeq ($(ARCH),riscv64)
    C_TARGET ?= riscv64-unknown-elf
endif

ifneq ($(C_TARGET),)
CFLAGS += --target=$(C_TARGET)
ASFLAGS += --target=$(C_TARGET)
endif
