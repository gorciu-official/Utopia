#include <syscall.h>

void _start() {
    char* shit = "launching shell\n";
    char* init_path = "/init2";
    __libc_syscall3(5, 2, (uintptr_t)shit, 16);
    __libc_syscall2(2001, (uintptr_t)init_path, 1);
    while (1) continue;
} 
