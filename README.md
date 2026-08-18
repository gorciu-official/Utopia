# Utopia

Utopia is a (probably good) x86_64 operating system kernel.

<div align="center">
    <a href="https://github.com/gorciu-official/Utopia/releases/latest">
        <img src="https://github.com/gorciu-official/Utopia/blob/main/assets/screenshot.png?raw=true">
    </a>
</div>

## Features

- runs quite well single-threaded Linux userspace static/dynamic executables
- has SMP (although it is quite useless, because of lack of per-CPU clock events, that's a big TODO) 
- multiprotocolar: supports Multiboot1 and Limine boot protocol

That's it lol. Please note that this is an hobbyist operating system and probably shouldn't be used for anything other than testing purposes, it also lacks a lot of features.

## Dependencies

- a normal assembler: NASM
- a C compiler 
- GNU make (or any other program that reads Makefiles)

## System requirements

Have a working x86_64 machine (kinda).

Jokes aside but should work everywhere if you have an x86_64 processor. Tested on my machine or in QEMU and everything mostly behaves as it should. 

## Building and testing 

For this kind of information refer to the [CONTRIBUTING.md](docs/CONTRIBUTING.md) file.

## Software

The kernel is being constantly adapted to run some software as system inits. 

**Currently "ported" software list:**

- [nsh](https://github.com/na-razie-bez-nicku/nsh)
- [JUAMP](https://github.com/gorciu-official/JUAMP)

Running any of these of course assume a good C standard library (glibc may work, musl is ideal). Support for dynamic executables was added somewhat recently, so yes - you should (in theory) be able to run them!

## Additional notes

- Contributions are welcome!
- Utopia is licensed under GNU GPL v3.0
