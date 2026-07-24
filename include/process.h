#pragma once

#include <types.h>
#include <scheduler.h>

#define MAX_PROCESSES 256
#define MAX_FILES_PER_PROCESS 32

struct vnode;

typedef struct file_desc {
    struct vnode* vnode;
    uint64_t offset;
    int flags;
    bool used;
} file_desc_t;

typedef struct process {
    uint32_t pid;
    char name[32];
    uint64_t* page_table; 
    uint64_t brk_start;
    uint64_t brk_current;
    uint64_t mmap_start;
    uint64_t mmap_current;
    struct thread* main_thread; 
    struct process* next;   
    file_desc_t fds[MAX_FILES_PER_PROCESS];
} process_t;

void process_init(void);

process_t* process_create(const char* name, void (*entry_point)(void*), void* arg, int ring);
void process_terminate(process_t* proc);
process_t* process_get_current(void);
