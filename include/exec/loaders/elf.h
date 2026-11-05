#pragma once

#include <types.h>

typedef struct {
    struct {
        const uint8_t* data;
        uintptr_t size;
    } image;

    struct {
        char** argv;
        int argc;
        char** envp;
    } args;

    int syscall_conv;
    int arg;
} create_elf_process_request_t;

int elf_start(create_elf_process_request_t req);
