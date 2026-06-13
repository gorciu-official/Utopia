#!/usr/bin/env -S bash --posix

qemu-system-x86_64 $@ -s -S & disown
gdb -ex "target remote :1234" ./iso/kernel.bin
