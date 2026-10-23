ifeq ($(ARCH),riscv64)
    QEMU := qemu-system-riscv64
endif
ifeq ($(ARCH),x86_64)
    QEMU := qemu-system-x86_64
endif

QEMU_FLAGS := -cdrom $(ISO_FILE) -serial stdio -m 1G

# smp 

SMP_ENABLED    ?= true
USE_HOST_CPU   ?= true
SMP_CORES      ?= 4

ifeq ($(SMP_ENABLED),true)
    QEMU_FLAGS += -smp $(SMP_CORES)
endif

# kvm acceleration 
ifeq ($(USE_HOST_CPU),true)
    QEMU_FLAGS += -enable-kvm -cpu host,invtsc=on
endif

# riscv64's UEFI firmware
ifeq ($(ARCH),riscv64)
    QEMU_FLAGS += -machine virt -device ramfb
    QEMU_FLAGS += -drive if=pflash,format=raw,unit=0,file=target/third-party.riscv64.edk2/RISCV_VIRT_CODE.fd,readonly=on
    QEMU_FLAGS += -drive if=pflash,format=raw,unit=1,file=target/third-party.riscv64.edk2/RISCV_VIRT_VARS.fd 
endif

# additional qemu flags provided by the user
EXTRA_QEMU_FLAGS ?=
QEMU_FLAGS += $(EXTRA_QEMU_FLAGS)

# convienence targets for running kernel in qemu 

run: all
	$(QEMU) $(QEMU_FLAGS)

run_dbg: all
	@chmod +x scripts/run_debug_mode.sh 
	./scripts/run_debug_mode.sh $(QEMU) $(KERNEL_BIN) $(QEMU_FLAGS)
