#!/usr/bin/env -S bash --posix

qemu="$1"
shift

kernel="$1"
shift

$qemu -s -S "$@" & disown
gdb -ex "target remote :1234" "$kernel"
