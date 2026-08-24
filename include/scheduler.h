#pragma once

#include <types.h>
#include <arch/x86_64/registers.h>

struct process; // smth like this works lol

typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED,
    THREAD_STATE_UNINITIALISED
} thread_state_t;

typedef struct thread {
    uint32_t id;
    char name[32];
    thread_state_t state;
    registers_t* stack_ptr; 
    void* stack_base;
    size_t stack_size;
    struct thread* next;
    int ring;
    struct process* process; 
} thread_t;

typedef struct {
    uint64_t at_entry;   // real program's entry point
    uint64_t at_phdr;    // vaddr of the real program's phdrs
    uint16_t at_phent;
    uint16_t at_phnum;
    uint64_t at_base;    // interpreter's load bias (0 if no interpreter)
    bool     has_interp;
} elf_auxv_info_t;

void scheduler_init(void);
void scheduler_ap_init(void);
thread_t* thread_create(const char* name, void (*entry_point)(void*), void* arg, int ring, elf_auxv_info_t* auxv);
registers_t* scheduler_schedule(registers_t* regs);
void thread_yield(void);
void thread_exit(void);
thread_t* scheduler_get_current_thread(void);
void scheduler_enqueue(thread_t* t);
