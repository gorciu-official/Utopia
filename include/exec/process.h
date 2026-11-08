#pragma once

#include <types.h>
#include <exec/scheduler.h>
#include <mm/vmm.h>

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
    uint32_t ppid;
    char name[32];
    uint64_t* page_table; 
    uint64_t brk_start;
    uint64_t brk_current;
    uint64_t mmap_start;
    uint64_t mmap_current;
    struct thread* main_thread; 
    struct process* next;   
    file_desc_t fds[MAX_FILES_PER_PROCESS];
    int syscall_conv;
    vm_page_list_t owned_pages;

    bool         zombie; 
    int          exit_status;  
    uint32_t     main_tid;
    wait_queue_t child_wq;  
} process_t;

void process_init(void);

process_t* process_create(const char* name, void (*entry_point)(void*), uint64_t arg, int ring, thread_stack_t* stack);
void process_terminate(process_t* proc);

int process_wait_child(process_t* parent, int pid, int* status_out);
process_t* process_get_current(void);

struct registers;
struct elf_prepared;

typedef struct {
    uint64_t child_stack;  
    bool     set_tls;   
    uint64_t tls;
    uint64_t child_tid_addr; 
} process_clone_opts_t;

process_t* process_clone(process_t* parent, thread_t* parent_thread, const struct registers* frame, const process_clone_opts_t* opts);

void process_replace(process_t* proc, thread_t* thread, struct registers* frame, struct elf_prepared* prepared, const char* name);
