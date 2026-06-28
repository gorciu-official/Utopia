#pragma once

#include <types.h>
#include <registers.h>

struct process; // smth like this works lol

typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED
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

void scheduler_init(void);
void scheduler_ap_init(void);
thread_t* thread_create(const char* name, void (*entry_point)(void*), void* arg, int ring);
registers_t* scheduler_schedule(registers_t* regs);
void thread_yield(void);
void thread_exit(void);
thread_t* scheduler_get_current_thread(void);
