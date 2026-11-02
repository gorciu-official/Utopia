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

CFLAGS         := $(shell tr '\n' ' ' < compile_flags.txt) -DUTOPIA_COMMIT=\"$(shell git rev-parse --short HEAD)\"
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

ifeq ($(DEBUG),true) 
    CFLAGS += -g -DUTOPIA_DEBUG=1
endif
