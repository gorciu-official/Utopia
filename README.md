# Utopia

Utopia is a (probably good) x86_64 operating system kernel.

<div align="center">
    <img width="783" height="566" alt="image" src="https://github.com/user-attachments/assets/64c4e884-17e4-4a95-8d9e-82f8a8ce3256" />
</div>

## Dependencies

- a normal assembler: NASM
- a C compiler 
- GNU make (or any other program that reads Makefiles)

## System requirements

Have a working x86_64 machine (kinda).

Jokes aside but should work everywhere if you have an x86_64 processor. Tested on my machine or in QEMU and everything mostly behaves as it should. GRUB will probably be the bottleneck when trying to t est the limits of the kernel, to be honest. Limine was introduced somewhat recently, but due to it crashing at multi-core systems its support is experimental until this is fixed fully.

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

## Additional notes

- Contributions are welcome!
- Utopia is licensed under GNU GPL v3.0
