# Building and running

## Dependencies

So, before building, make sure you have these:

- a good assembler (NASM) for x86_64 / a bad assembler (GNU as) for non-x86_64 architectures
- a C compiler 
- GNU make (or any other program that reads Makefiles)

If you want to test on an emulated machine make sure you have a good CPU (good cpu = smth better than core 2 quad q9300) and QEMU installed.

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

### Bootloaders

There are two bootloaders that are supported natively - Limine and GRUB (default is Limine, but you can change that with `BOOTLOADER=grub`). After tweaking the Makefile, you should be able to run the kernel on any bootloader that supports either Limine boot protocol or Multiboot1. 

### Architectures

There are currently two supported architectures - `x86_64` and `RISC-V 64`. Default one is `x86_64`, but this can be changed to RISC-V by using `ARCH=riscv64`. 

If you want to use QEMU with an Utopia build architecture different from your host system, you will need to disable KVM with `USE_HOST_CPU=false`. This also applies to running Utopia in QEMU on Windows (but Makefile is written for Unix-like systems, so you may have issues regardless).

Support levels for certain architecture & boot protocol pairs are listed in `README.md`.

## Rebuilding bootloader configuration

By default, `make` does it the first time you try to compile the project. But if you for some reason need to rebuild it again (e.g. after removing initramfs or adding a custom font), you will need to execute these commands:

```
chmod +x scripts/mk_bootloader_cfg.sh 
./scripts/mk_bootloader_cfg.sh
```

Alternatively, you may just delete old configuration files from `src/build/`, specifically `limine.conf` and `grub.cfg`, then rebuild the kernel.
