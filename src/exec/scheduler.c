#include <arch/memory.h>
#include <arch/common.h>
#include <exec/scheduler.h>
#include <exec/process.h>
#include <memory.h>
#include <lib/screen.h>
#include <lib/string.h>
#include <lib/spinlock.h>
#include <panic.h>
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#include <arch/x86_64/msr.h>
#endif

static spinlock_t scheduler_lock;

static thread_t* current_threads[CPU_ARCH_MAX_CPUS];
static thread_t* idle_threads[CPU_ARCH_MAX_CPUS];
static uintptr_t boot_cr3[CPU_ARCH_MAX_CPUS]; 
static uintptr_t active_cr3[CPU_ARCH_MAX_CPUS]; 

static thread_t* ready_queue_head = NULL;
static thread_t* ready_queue_tail = NULL;
static thread_t* zombie_list = NULL; 
static thread_t* pending_zombie[CPU_ARCH_MAX_CPUS];
static thread_t* pending_park[CPU_ARCH_MAX_CPUS];
static uint32_t  next_thread_id = 0;

bool context_switch_lock = false;

static inline unsigned long sched_lock(void) {
    unsigned long flags = arch_save_interrupts();
    spinlock_acquire(&scheduler_lock);
    return flags;
}

static inline uintptr_t read_page_table_root(void) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    uintptr_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
#endif 
    // TODO: RISC-V handling
    return 0;
}

static inline void sched_unlock(unsigned long flags) {
    spinlock_release(&scheduler_lock);
    arch_restore_interrupts(flags);
}

static void enqueue_locked(thread_t* t) {
    t->state = THREAD_STATE_READY;
    t->next = NULL;
    if (ready_queue_tail == NULL) {
        ready_queue_head = t;
        ready_queue_tail = t;
    } else {
        ready_queue_tail->next = t;
        ready_queue_tail = t;
    }
}

static thread_t* dequeue_locked(void) {
    thread_t* t = ready_queue_head;
    if (t != NULL) {
        ready_queue_head = t->next;
        if (ready_queue_head == NULL)
            ready_queue_tail = NULL;
        t->next = NULL;
    }
    return t;
}

static inline uintptr_t kernel_stack_top(thread_t* t) {
    if (!t || !t->stacks.kernel)
        return 0; // 0 = use the per-CPU default stack
    return ((uintptr_t)t->stacks.kernel->base + t->stacks.kernel->size) & ~(uintptr_t)0xF;
}

static thread_stack_t* kernel_stack_alloc(void) {
    thread_stack_t* ks = (thread_stack_t*)malloc(sizeof(thread_stack_t));
    if (!ks) return NULL;

    ks->size = THREAD_KERNEL_STACK_SIZE;
    ks->base = malloc(ks->size);
    if (!ks->base) {
        free(ks);
        return NULL;
    }
    memset(ks->base, 0, ks->size);
    ks->ptr = (void*)((uintptr_t)ks->base + ks->size);
    return ks;
}

static void thread_set_name(thread_t* t, const char* name) {
    size_t i = 0;
    if (name) {
        for (; i < sizeof(t->name) - 1 && name[i]; i++)
            t->name[i] = name[i];
    }
    t->name[i] = '\0';
}

static thread_t* tcb_alloc(const char* name) {
    thread_t* t = (thread_t*)malloc(sizeof(thread_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(thread_t));

    t->regs = (registers_t*)malloc(sizeof(registers_t));
    if (!t->regs) {
        free(t);
        return NULL;
    }
    memset(t->regs, 0, sizeof(registers_t));

    t->id = __atomic_fetch_add(&next_thread_id, 1, __ATOMIC_RELAXED);
    thread_set_name(t, name);
    t->state = THREAD_STATE_UNINITIALISED;
    t->next = NULL;
    t->process = NULL;
    return t;
}

void scheduler_reap(void) {
    if (!__atomic_load_n(&zombie_list, __ATOMIC_ACQUIRE))
        return;

    unsigned long flags = sched_lock();
    thread_t* list = zombie_list;
    zombie_list = NULL;
    sched_unlock(flags);

    while (list) {
        thread_t* next = list->next;
        if (list->stacks.kernel) {
            free(list->stacks.kernel->base);
            free(list->stacks.kernel);
        }
        if (list->stacks.main) {
            free(list->stacks.main->base);
            free(list->stacks.main);
        }
        if (list->regs)          free(list->regs);
        free(list);
        list = next;
    }
}

static void register_idle_for_this_cpu(thread_t* t) {
    uint32_t cpu_id = current_processor_id();

    t->state = THREAD_STATE_RUNNING;
    boot_cr3[cpu_id]   = read_page_table_root();
    active_cr3[cpu_id] = boot_cr3[cpu_id];

    current_threads[cpu_id] = t;
    idle_threads[cpu_id]    = t;
}

void scheduler_init(void) {
    spinlock_init(&scheduler_lock);

    thread_t* main_thread = tcb_alloc("main");
    if (!main_thread) {
        printk(KMT_LOG, "Scheduler", "Failed to allocate main thread TCB!");
        return;
    }
    register_idle_for_this_cpu(main_thread);
}

void scheduler_ap_init(void) {
    uint32_t cpu_id = current_processor_id();

    char name[5] = { 'a', 'p', '_', (char)('0' + (cpu_id % 10)), '\0' };
    thread_t* ap_thread = tcb_alloc(name);
    if (!ap_thread) {
        printk(KMT_LOG, "Scheduler", "Failed to allocate AP thread TCB for CPU %d!", cpu_id);
        return;
    }
    register_idle_for_this_cpu(ap_thread);
}

void scheduler_enqueue(thread_t* t) {
    unsigned long flags = sched_lock();
    enqueue_locked(t);
    sched_unlock(flags);
}

thread_t* thread_create(const char* name, void (*entry_point)(void*), uint64_t arg, int ring,
                        thread_stack_t* stack) {
    scheduler_reap();

    thread_t* t = tcb_alloc(name);
    if (!t) {
        printk(KMT_LOG, "Scheduler", "Failed to allocate TCB for new thread '%s'!", name);
        return NULL;
    }

    if (ring == 3) {
        t->stacks.kernel = kernel_stack_alloc();
        if (!t->stacks.kernel) {
            printk(KMT_LOG, "Scheduler", "Failed to allocate kernel stack for thread '%s'!", name);
            free(t->regs);
            free(t);
            return NULL;
        }
    }

    if (ring == 3) {
        uintptr_t start = (uintptr_t)stack->base & ~(uintptr_t)0xFFF;
        uintptr_t end   = ((uintptr_t)stack->base + stack->size + 0xFFF) & ~(uintptr_t)0xFFF;
        for (uintptr_t a = start; a < end; a += 4096) {
            set_page_permissions(a, VMF_USER | VMF_WRITE);
        }
    }

    t->stacks.main = stack;

    registers_t* regs = t->regs;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    regs->rip    = (uintptr_t)entry_point;
    regs->cs     = ring == 0 ? 0x08 : (0x28 | 3);
    regs->ss     = ring == 0 ? 0x10 : (0x20 | 3);
    regs->rflags = 0x202;
    regs->rsp    = (uintptr_t)stack->ptr;
    regs->rdi    = arg; 
    regs->rax    = arg;  
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    regs->x[1]  = (uintptr_t)thread_exit; /* ra */
    regs->x[2]  = (uintptr_t)stack->ptr;
    regs->x[10] = arg;
    regs->sepc  = (uintptr_t)entry_point;
    regs->sstatus &= ~(3ULL << 13);
    regs->sstatus |=  (1ULL << 13); // FS=Initial
#endif

    return t;
}

registers_t* scheduler_schedule(registers_t* regs) {
    if (context_switch_lock)
        return regs;

    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];
    if (!curr || !regs)
        return regs;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    bool interrupted_kernel = (regs->cs & 3) == 0;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    bool interrupted_kernel = (regs->sstatus & (1UL << 8)) != 0; /* SPP */
#endif
    if (interrupted_kernel && curr->process != NULL && curr->state != THREAD_STATE_BLOCKED)
        return regs;

    if (curr->regs) {
        memcpy(curr->regs, regs, sizeof(registers_t));
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        if (interrupted_kernel) {
            curr->regs->rsp = (uint64_t)&regs->rsp;
            curr->regs->ss  = 0x10;
        }
#endif
    }

    unsigned long flags = sched_lock();

    if (pending_park[cpu_id]) {
        thread_t* t = pending_park[cpu_id];
        pending_park[cpu_id] = NULL;
        wait_queue_t* wq = t->waiting_on;
        if (!wq || wq->seq != t->wait_seq) {
            t->waiting_on = NULL; 
            enqueue_locked(t);
        } else {
            t->next  = wq->head;
            wq->head = t;
        }
    }

    if (pending_zombie[cpu_id]) {
        pending_zombie[cpu_id]->next = zombie_list;
        zombie_list = pending_zombie[cpu_id];
        pending_zombie[cpu_id] = NULL;
    }

    thread_t* idle = idle_threads[cpu_id];

    if (curr->state == THREAD_STATE_RUNNING && curr != idle)
        enqueue_locked(curr);

    thread_t* next = dequeue_locked();
    if (!next)
        next = idle ? idle : curr;

    if (next == curr) {
        curr->waiting_on = NULL;
        curr->state = THREAD_STATE_RUNNING;
        sched_unlock(flags);
        return regs;
    }

    if (curr->state == THREAD_STATE_BLOCKED)
        pending_park[cpu_id] = curr;

    if (curr->state == THREAD_STATE_TERMINATED && curr != idle) {
        pending_zombie[cpu_id] = curr;
    }

    next->state = THREAD_STATE_RUNNING;
    current_threads[cpu_id] = next;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    if (curr->process) curr->fs_base = read_msr(IA32_FS_BASE);
    if (next->process) write_msr(IA32_FS_BASE, next->fs_base);
#endif

    arch_set_kernel_stack(cpu_id, kernel_stack_top(next));

    uintptr_t cr3 = 0;
    bool force = false;
    if (next->process && next->process->page_table) {
        cr3   = hhdm_virt_to_phys(next->process->page_table);
        force = true;
    } else {
        cr3 = boot_cr3[cpu_id];
    }

    sched_unlock(flags);

    if (cr3 && (force || cr3 != active_cr3[cpu_id])) {
        write_cr3(cr3);
        active_cr3[cpu_id] = cr3;
    }

    return next->regs;
}

void thread_exit(void) {
    unsigned long flags = arch_save_interrupts();
    (void)flags; 

    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    if (!curr || curr == idle_threads[cpu_id]) {
        arch_restore_interrupts(flags);
        return;
    }

    if (curr->process != NULL) {
        if (curr->process->pid == 1) {
            panic("Init process has exited or has been killed", NULL);
            return;
        }

        if (boot_cr3[cpu_id]) {
            write_cr3(boot_cr3[cpu_id]);
            active_cr3[cpu_id] = boot_cr3[cpu_id];
        }

        process_terminate(curr->process);
        curr->process = NULL;
    }

    curr->state = THREAD_STATE_TERMINATED;

    arch_restore_interrupts(flags);
    for (;;)
        arch_wfi();
}

uint64_t wait_queue_snapshot(wait_queue_t* wq) {
    return __atomic_load_n(&wq->seq, __ATOMIC_ACQUIRE);
}

void scheduler_wait(wait_queue_t* wq, uint64_t seq) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    unsigned long flags = arch_save_interrupts();

    if (__atomic_load_n(&wq->seq, __ATOMIC_ACQUIRE) != seq) {
        arch_restore_interrupts(flags);
        return;
    }

    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    if (!curr || curr == idle_threads[cpu_id]) {
        __asm__ volatile("sti; hlt; cli" ::: "memory");
        arch_restore_interrupts(flags);
        return;
    }

    curr->waiting_on = wq;
    curr->wait_seq   = seq;
    curr->state      = THREAD_STATE_BLOCKED;

    arch_invi(0x41);

    if (curr->state == THREAD_STATE_BLOCKED) {
        curr->waiting_on = NULL;
        curr->state = THREAD_STATE_RUNNING;
    }
    arch_restore_interrupts(flags);
#else
    (void)wq; (void)seq;
    arch_wfi();
#endif
}

void scheduler_wake_all(wait_queue_t* wq) {
    unsigned long flags = sched_lock();

    wq->seq++;
    thread_t* t = wq->head;
    wq->head = NULL;
    while (t) {
        thread_t* next = t->next;
        t->waiting_on = NULL;
        enqueue_locked(t);
        t = next;
    }

    sched_unlock(flags);
}

thread_t* scheduler_get_current_thread(void) {
    return current_threads[current_processor_id()];
}
