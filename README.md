# Utopia

Utopia is a (probably good) 64-bit operating system kernel.

<div align="center">
    <a href="https://github.com/gorciu-official/Utopia/releases/latest">
        <img src="https://github.com/gorciu-official/Utopia/blob/main/docs/assets/screenshot.png?raw=true">
    </a>
</div>

## Features

- runs quite well single-threaded Linux userspace static/dynamic executables
- has SMP 
- multiprotocolar: supports Multiboot1 and Limine boot protocol

That's it lol. Please note that this is an hobbyist operating system and probably shouldn't be used for anything other than testing purposes, it also lacks a lot of features.

## System requirements

Have a working `x86_64`/`RISC-V 64` machine (kinda).

Jokes aside but should work everywhere (with a few exceptions of course). Tested on my machine or in QEMU and everything mostly behaves as it should. 

RISC-V support is experimental, so don't mind something not working.

### Support levels

| Architecture | Boot protocol | Support level | Note                                                                             |
| ------------ | ------------- | ------------- | -------------------------------------------------------------------------------- |
| x86_64       | Limine        | Full          | Dev time mostly focused on this build.                                           |
| x86_64       | Multiboot1    | Partial       | SMP triple faulting; not detecting init file in initramfs.                       |
| RISC-V 64    | Limine        | Partial       | No SMP; userspace is in progress.                                                |
| RISC-V 64    | Multiboot1    | Unsupported   | Trying to get Limine version to boot first.                                      |

I copied the idea of this table from [here](https://git.evalyngoemer.com/evalynOS/evalynOS).

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
