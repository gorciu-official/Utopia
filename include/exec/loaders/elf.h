#pragma once

#include <types.h>
#include <memory.h>
#include <exec/scheduler.h>

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

typedef struct elf_prepared {
    uint64_t*        l4;
    uint64_t         entry;  
    uint64_t         sp;
    uint64_t         brk_start;
    thread_stack_t*  stack; 
    vm_page_list_t   pages;  
} elf_prepared_t;

int  elf_prepare(const create_elf_process_request_t* req, elf_prepared_t* out);
void elf_prepared_destroy(elf_prepared_t* prepared);

int elf_start(create_elf_process_request_t req);
