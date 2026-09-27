#include <syscall.h>

void _start() {
    char* shit = "hi\n";
    char* init_path = "/init/";
    __libc_syscall3(5, 2, (uintptr_t)shit, 3);
    __libc_syscall1(2001, (uintptr_t)init_path);
    while (1) continue;
} 
