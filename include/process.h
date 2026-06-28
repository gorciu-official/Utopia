#pragma once

#include <types.h>
#include <scheduler.h>

#define MAX_PROCESSES 256

typedef struct process {
    uint32_t pid;
    char name[32];
    uint64_t* page_table; 
    struct thread* main_thread; 
    struct process* next;   
} process_t;

void process_init(void);

process_t* process_create(const char* name, void (*entry_point)(void*), void* arg, int ring);
void process_terminate(process_t* proc);
process_t* process_get_current(void);
