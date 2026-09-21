#include <syscall.h>

void _start() {
    char* shit = "hi\n";
    __libc_syscall3(1, 2, (uintptr_t)shit, 2);
    __libc_syscall0(60);
} 
