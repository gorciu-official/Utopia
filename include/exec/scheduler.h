#pragma once

#include <types.h>
#include <arch/common.h>

typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED,
    THREAD_STATE_UNINITIALISED
} thread_state_t;

#define THREAD_KERNEL_STACK_SIZE (16 * 1024)

typedef struct {
    void*  ptr; 
    void*  base;
    size_t size;
} thread_stack_t;

struct thread;

typedef struct wait_queue {
    struct thread* head;
    uint64_t       seq;
} wait_queue_t;

#define WAIT_QUEUE_INIT { NULL, 0 }

typedef struct thread {
    // base
    uint32_t id;
    char name[32];
    thread_state_t state;
    struct process* process;
    uint64_t      fs_base;

    // stack
    struct {
        thread_stack_t* main;
        thread_stack_t* kernel;
    } stacks;

    // saved state
    int ring;
    registers_t* regs;

    // blocking
    wait_queue_t* waiting_on;
    uint64_t      wait_seq;

    // list
    struct thread* next;
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
thread_t* thread_create(const char* name, void (*entry_point)(void*), uint64_t arg, int ring, thread_stack_t* stack);
registers_t* scheduler_schedule(registers_t* regs);
void thread_exit(void);
thread_t* scheduler_get_current_thread(void);
void scheduler_enqueue(thread_t* t);
void scheduler_reap(void);

uint64_t wait_queue_snapshot(wait_queue_t* wq);
void scheduler_wait(wait_queue_t* wq, uint64_t seq);
void scheduler_wake_all(wait_queue_t* wq);
