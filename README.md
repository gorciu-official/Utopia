# Utopia

Utopia is a (probably good) x86_64 operating system kernel.

<div align="center">
    <img width="936" height="1026" alt="image" src="https://github.com/user-attachments/assets/1f4c2f7f-4b5a-458c-87f8-91102386bfbd" />
</div>

## Features

- runs quite well single-threaded Linux userspace static executables (there is no support for dynamic ones yet, sorry)
- has SMP (although it is quite useless, because of lack of per-CPU clock events, that's a big TODO) 
- multiprotocolar: supports Multiboot1 and Limine boot protocol

That's it lol. Please note that this is an hobbyist operating system and probably shouldn't be used for anything other than testuing purposes, it also lacks a lot of features.

## Dependencies

- a normal assembler: NASM
- a C compiler 
- GNU make (or any other program that reads Makefiles)

## System requirements

Have a working x86_64 machine (kinda).

Jokes aside but should work everywhere if you have an x86_64 processor. Tested on my machine or in QEMU and everything mostly behaves as it should. 

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

There are two bootloaders that are supported natively - Limine and GRUB (default is Limine, but you can change that with `BOOTLOADER=grub`). After tweaking the Makefile, you should be able to run the kernel on any bootloader that supports either Limine boot protocol or Multiboot1. Multiboot1 booting used to be the primary option, but now has unresolved issues - please wait a while or use a different boot protocol.

## Additional notes

- Contributions are welcome!
- Utopia is licensed under GNU GPL v3.0
