# Contributing

This file contains some of the important information for (I guess) new contributors.

## Dependencies

So, before building, make sure you have these:

- a normal assembler: NASM
- a C compiler 
- GNU make (or any other program that reads Makefiles)

If you want to test on an emulated machine make sure you have a good CPU (good cpu = smth better than core 2 quad q9300) and QEMU installed.

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

There are two bootloaders that are supported natively - Limine and GRUB (default is Limine, but you can change that with `BOOTLOADER=grub`). After tweaking the Makefile, you should be able to run the kernel on any bootloader that supports either Limine boot protocol or Multiboot1. Multiboot1 booting used to be the primary option, but now has unresolved issues - please wait a while or use a different boot protocol.

## Rebuilding bootloader configuration

By default, `make` does it the first time you try to compile the project. But if you for some reason need to rebuild it again (e.g. after removing initramfs or adding a custom font), you will need to execute these commands:

```
chmod +x scripts/mk_bootloader_cfg.sh 
./scripts/mk_bootloader_cfg.sh
```

Alternatively, you may just delete old configuration files from `src/build/`, specifically `limine.conf` and `grub.cfg`, then rebuild the kernel.
