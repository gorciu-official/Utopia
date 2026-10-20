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
- kinda modular: supports multiple boot protocols (Multiboot1, Limine) and multiple CPU architectures (x86_64, RISC-V 64)

That's it lol. Please note that this is an hobby operating system kernel and probably shouldn't be used for anything other than testing purposes, it also lacks a lot of features.

## System requirements

Have a working `x86_64`/`RISC-V 64` machine (kinda).

Jokes aside but should work everywhere (with a few exceptions of course). Tested on my machine or in QEMU and everything mostly behaves as it should. 

### Support levels

| Architecture | Boot protocol | Support level | Note                                                                             |
| ------------ | ------------- | ------------- | -------------------------------------------------------------------------------- |
| x86_64       | Limine        | Full          | -                                                                                |
| x86_64       | Multiboot1    | Partial       | SMP triple faulting; not detecting init file in initramfs.                       |
| RISC-V 64    | Limine        | Full          | -                                                                                |
| RISC-V 64    | Multiboot1    | Unsupported   | Trying to get Limine version to boot first.                                      |

I copied the idea of this table from [here](https://git.evalyngoemer.com/evalynOS/evalynOS).

## Running the project

You can simply [download the latest release from GitHub](https://github.com/gorciu-official/utopia/releases/latest).

If you want to compile the project yourself, refer to the [docs/BUILDING.md](docs/BUILDING.md) file. If you also want to contribute back, the [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) file is worth reading too.

## Software

The kernel is being constantly adapted to run some software as system inits. 

**Currently "ported" software list:**

- [nsh](https://github.com/na-razie-bez-nicku/nsh)
- [JUAMP](https://github.com/gorciu-official/JUAMP)

**In active porting:**

- [bash](https://www.gnu.org/software/bash/)
- certain parts of [inetutils](https://www.gnu.org/software/inetutils/)

Running any of these of course assume a good C standard library. Porting is primairly done on [glibc](https://www.gnu.org/software/libc/), although running simpler, but complete ones like [Musl](https://en.wikipedia.org/wiki/Musl) or [mlibc](https://github.com/managarm/mlibc) should also work. Support for dynamic executables was added somewhat recently, so yes - you should (in theory) be able to run them!

> [!CAUTION]
> In modern Utopia systems, an executable running through the Linux compatibility layer designed to run as a system init **MUST** be launched by a system init wrapper executable using the [Utopia syscall ABI](docs/SYSCALLS.md). I recommend writting a quick assembly wrapper. **This is a breaking change.**

## Additional notes

- Contributions are welcome!
- Main repository is [on GitHub](https://github.com/gorciu-official/Utopia), official mirrors: [Radicle](https://radicle.network/nodes/iris.radicle.network/rad%3Azxi5tifexnTkPWHimgBqHCqHLTpJ), [Evalyn's Git Server](https://git.evalyngoemer.com/gorciu/Utopia), [Gitea on tilde.town](https://git.tilde.town/gorciu/Utopia)
- Utopia is licensed under GNU GPL v3.0
