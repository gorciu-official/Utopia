#!/usr/bin/env -S bash --posix

qemu-system-x86_64 -cdrom Utopia.iso -serial stdio -s -S & disown
gdb -ex "target remote :1234" ./iso/boot/kernel.bin
