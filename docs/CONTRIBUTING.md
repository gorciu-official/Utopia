# Contributing

This file contains some of the important information for (I guess) new contributors.

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

There are two bootloaders that are supported natively - Limine and GRUB (default is Limine, but you can change that with `BOOTLOADER=grub`). After tweaking the Makefile, you should be able to run the kernel on any bootloader that supports either Limine boot protocol or Multiboot1. Multiboot1 booting used to be the primary option, but now has unresolved issues - please wait a while or use a different boot protocol.

## Resolving issues 

So if you want to resolve an issue, just comment that you are handling this, so no one else claims that issue.  If you are new. check issues with the `good first issue` tag.

That is really it.
