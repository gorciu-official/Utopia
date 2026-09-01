#!/usr/bin/env -S bash --posix

kernel="$1"
shift

qemu-system-x86_64 -s -S "$@" & disown
gdb -ex "target remote :1234" "$kernel"
