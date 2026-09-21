#include <syscall.h>

void _start() {
    char* shit = "hi\n";
    __libc_syscall3(5, 2, (uintptr_t)shit, 3);
    __libc_syscall0(2000);
} 
