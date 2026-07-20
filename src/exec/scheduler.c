#include <scheduler.h>
#include <process.h>
#include <memory.h>
#include <arch/x86_64/common.h>
#include <lib/screen.h>
#include <lib/string.h>
#include <lib/spinlock.h>

static spinlock_t scheduler_lock;
static thread_t* current_threads[CPU_ARCH_MAX_CPUS] = {NULL};
static thread_t* idle_threads[CPU_ARCH_MAX_CPUS] = {NULL};
static thread_t* ready_queue_head = NULL;
static thread_t* ready_queue_tail = NULL;
static thread_t* garbage_list = NULL;
static uint32_t next_thread_id = 0;

static void scheduler_enqueue(thread_t* t) {
    spinlock_acquire(&scheduler_lock);
    t->state = THREAD_STATE_READY;
    t->next = NULL;
    if (ready_queue_tail == NULL) {
        ready_queue_head = t;
        ready_queue_tail = t;
    } else {
        ready_queue_tail->next = t;
        ready_queue_tail = t;
    }
    spinlock_release(&scheduler_lock);
}

static thread_t* scheduler_dequeue(void) {
    spinlock_acquire(&scheduler_lock);
    thread_t* t = ready_queue_head;
    if (t != NULL) {
        ready_queue_head = t->next;
        if (ready_queue_head == NULL) {
            ready_queue_tail = NULL;
        }
        t->next = NULL;
    }
    spinlock_release(&scheduler_lock);
    return t;
}

void scheduler_init(void) {
    spinlock_init(&scheduler_lock);
    
    uint32_t cpu_id = current_processor_id();

    thread_t* main_thread = (thread_t*)malloc(sizeof(thread_t));
    if (!main_thread) {
        printk("Scheduler", "Failed to allocate main thread TCB!");
        return;
    }

    main_thread->id = next_thread_id++;
    strcpy(main_thread->name, "main");
    main_thread->state = THREAD_STATE_RUNNING;
    main_thread->stack_base = NULL;
    main_thread->stack_ptr = NULL;
    main_thread->next = NULL;
    main_thread->process = NULL;

    current_threads[cpu_id] = main_thread;
    idle_threads[cpu_id] = main_thread;
}

void scheduler_ap_init(void) {
    uint32_t cpu_id = current_processor_id();

    thread_t* ap_thread = (thread_t*)malloc(sizeof(thread_t));
    if (!ap_thread) {
        printk("Scheduler", "Failed to allocate AP thread TCB for CPU %d!", cpu_id);
        return;
    }

    ap_thread->id = next_thread_id++;
    ap_thread->name[0] = 'a';
    ap_thread->name[1] = 'p';
    ap_thread->name[2] = '_';
    ap_thread->name[3] = '0' + (cpu_id % 10);
    ap_thread->name[4] = '\0';
    
    ap_thread->state = THREAD_STATE_RUNNING;
    ap_thread->stack_base = NULL;
    ap_thread->stack_ptr = NULL;
    ap_thread->next = NULL;
    ap_thread->process = NULL;

    current_threads[cpu_id] = ap_thread;
    idle_threads[cpu_id] = ap_thread;
}

thread_t* thread_create(const char* name, void (*entry_point)(void*), void* arg, int ring) {
    thread_t* t = (thread_t*)malloc(sizeof(thread_t));
    if (!t) {
        printk("Scheduler", "Failed to allocate TCB for new thread '%s'!", name);
        return NULL;
    }

    size_t stack_size = 8192;
    void* stack_base = malloc(stack_size);

    if (!stack_base) {
        printk("Scheduler", "Failed to allocate stack for new thread '%s'!", name);
        free(t);
        return NULL;
    }

    if (ring == 3) {
        for (size_t i = 0; i < stack_size; i += 4096) {
            set_page_permissions(
                (uintptr_t)stack_base + i,
                PAGE_RW | PAGE_USER | PAGE_PRESENT
            );
        }
    }

    t->id = next_thread_id++;
    strcpy(t->name, name);
    t->state = THREAD_STATE_READY;
    t->stack_base = stack_base;
    t->stack_size = stack_size;
    t->next = NULL;
    t->process = NULL;

    uint64_t sp = (uint64_t)stack_base + stack_size;

    sp &= ~15ULL;

    if (ring == 3) { 
        sp -= 16;
        uint64_t random_addr = sp;

        ((uint8_t*)random_addr)[0] = 0x12;
        ((uint8_t*)random_addr)[1] = 0x34;

        // auxv: AT_NULL
        sp -= 8;
        *(uint64_t*)sp = 0;
        sp -= 8;
        *(uint64_t*)sp = 0;

        // auxv: AT_RANDOM
        sp -= 8;
        *(uint64_t*)sp = random_addr; 
        sp -= 8;
        *(uint64_t*)sp = 25; 

        // envp[0] = NULL
        sp -= 8;
        *(uint64_t*)sp = 0;

        // argv[1] = NULL
        sp -= 8;
        *(uint64_t*)sp = 0;

        // argv[0]
        sp -= 8;
        *(uint64_t*)sp = (uint64_t)arg;

        // argc = 1
        sp -= 8;
        *(uint64_t*)sp = 1;

        sp &= ~15ULL;
    }

    registers_t* regs =
        (registers_t*)(sp - sizeof(registers_t));

    memset(regs, 0, sizeof(registers_t));

    regs->rip = (uint64_t)entry_point;

    if (ring == 3) {
        regs->rdi = 0;
        regs->rsi = 0;
        regs->rdx = 0;
    }
    else {
        regs->rdi = (uint64_t)arg;
    }

    regs->cs = ring == 0 ? 0x08 : (0x28 | 3);
    regs->ss = ring == 0 ? 0x10 : (0x20 | 3);
    regs->rflags = 0x202;
    regs->rsp = sp;
    t->stack_ptr = regs;

    scheduler_enqueue(t);
    return t;
}

registers_t* scheduler_schedule(registers_t* regs) {
    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    if (!curr) return regs;

    thread_t* garbage = NULL;
    spinlock_acquire(&scheduler_lock);
    garbage = garbage_list;
    garbage_list = NULL;
    spinlock_release(&scheduler_lock);

    while (garbage) {
        thread_t* next_garbage = garbage->next;
        if (garbage->stack_base) {
            free(garbage->stack_base);
        }
        free(garbage);
        garbage = next_garbage;
    }

    curr->stack_ptr = regs;

    if (curr->state == THREAD_STATE_RUNNING && curr != idle_threads[cpu_id]) {
        curr->state = THREAD_STATE_READY;
        scheduler_enqueue(curr);
    }

    thread_t* next = scheduler_dequeue();
    if (!next) {
        if (curr->state != THREAD_STATE_TERMINATED) {
            curr->state = THREAD_STATE_RUNNING;
            return regs;
        }
        next = idle_threads[cpu_id];
    }

    next->state = THREAD_STATE_RUNNING;
    current_threads[cpu_id] = next;

    return next->stack_ptr;
}

void thread_yield(void) {
    asm volatile("int $32");
}

void thread_exit(void) {
    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    if (curr && curr != idle_threads[cpu_id]) {
        if (curr->process != NULL) {
            process_terminate(curr->process);
            curr->process = NULL;
        }

        curr->state = THREAD_STATE_TERMINATED;

        spinlock_acquire(&scheduler_lock);
        curr->next = garbage_list;
        garbage_list = curr;
        spinlock_release(&scheduler_lock);
    }

    thread_yield();

    while (true) {
        asm volatile("hlt");
    }
}

thread_t* scheduler_get_current_thread(void) {
    uint32_t cpu_id = current_processor_id();
    return current_threads[cpu_id];
}
