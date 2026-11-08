#include <exec/scheduler.h>
#include <exec/process.h>
#include <exec/loaders/elf.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <arch/memory.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <lib/string.h>
#include <lib/spinlock.h>
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#include <arch/x86_64/msr.h>
#endif

static spinlock_t process_lock;
static process_t* process_list_head = NULL;
static uint32_t next_pid = 1; 

void process_init(void) {
    spinlock_init(&process_lock);
    process_list_head = NULL;
}

process_t* process_create(const char* name, void (*entry_point)(void*), uint64_t arg, int ring, thread_stack_t* stack) {
    process_t* proc = (process_t*)malloc(sizeof(process_t));
    if (!proc) {
        printk(KMT_LOG, "Process", "Failed to allocate PCB for '%s'!", name);
        return NULL;
    }

    uint64_t* pt = clone_page_table();
    if (!pt) {
        printk(KMT_LOG, "Process", "Failed to clone page table for '%s'!", name);
        free(proc);
        return NULL;
    }

    memset(proc, 0, sizeof(process_t));
    spinlock_acquire(&process_lock);
    proc->pid = next_pid++;
    spinlock_release(&process_lock);
    proc->ppid = 0;
    strcpy(proc->name, name);
    proc->page_table = pt;

    thread_t* main_thread = thread_create(name, entry_point, arg, ring, stack);
    if (!main_thread) {
        printk(KMT_LOG, "Process", "Failed to create main thread for '%s'!", name);
        free_page_table(pt);
        free(proc);
        return NULL;
    }
    main_thread->process = proc;
    proc->main_thread = main_thread;
    proc->brk_start = 0;
    proc->brk_current = 0;
    proc->mmap_start = 0x400000000000;
    proc->mmap_current = 0x400000000000;

    memset(proc->fds, 0, sizeof(proc->fds));
    proc->fds[0].used = true;
    proc->fds[1].used = true;
    proc->fds[2].used = true;

    spinlock_acquire(&process_lock);
    proc->next = process_list_head;
    process_list_head = proc;
    spinlock_release(&process_lock);

    return proc;
}

void process_terminate(process_t* proc) {
    if (!proc) return;

    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        proc->fds[i].used = false;
        proc->fds[i].vnode = NULL;
    }

    if (proc->main_thread) {
        proc->main_thread->state = THREAD_STATE_TERMINATED;
        proc->main_thread->process = NULL; 
    }

    if (proc->page_table) {
        if (page_table_l4) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
            write_cr3(kernel_page_table_phys());
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
            uint64_t satp = (9ULL << 60) | (kernel_page_table_phys() >> 12);
            __asm__ volatile("csrw satp, %0\nsfence.vma\n" :: "r"(satp) : "memory");
#endif
        }
        free_page_table(proc->page_table);
        proc->page_table = NULL;
    }
    vm_page_list_free(&proc->owned_pages);

    proc->main_thread = NULL;

    unsigned long irq = arch_save_interrupts();
    spinlock_acquire(&process_lock);

    process_t* init = NULL;
    for (process_t* p = process_list_head; p; p = p->next)
        if (p->pid == 1 && !p->zombie && p != proc) init = p;

    bool init_has_new_zombie = false;
    for (process_t* p = process_list_head; p; p = p->next) {
        if (p == proc || p->ppid != proc->pid) continue;
        p->ppid = init ? 1 : 0;
        if (init && p->zombie) init_has_new_zombie = true;
    }

    process_t** pp = &process_list_head;
    while (*pp) {
        process_t* p = *pp;
        if (p != proc && p->zombie && p->ppid == 0) { *pp = p->next; free(p); }
        else pp = &p->next;
    }
    if (init && init_has_new_zombie) scheduler_wake_all(&init->child_wq);

    process_t* parent = NULL;
    if (proc->ppid) {
        for (process_t* p = process_list_head; p; p = p->next)
            if (p->pid == proc->ppid && !p->zombie) { parent = p; break; }
    }

    if (parent) {
        proc->zombie = true;
        scheduler_wake_all(&parent->child_wq);
        spinlock_release(&process_lock);
        arch_restore_interrupts(irq);
        return;
    }

    pp = &process_list_head;
    while (*pp && *pp != proc) pp = &(*pp)->next;
    if (*pp) *pp = proc->next;
    spinlock_release(&process_lock);
    arch_restore_interrupts(irq);

    free(proc);
}

int process_wait_child(process_t* parent, int pid, int* status_out) {
    if (!parent) return -1;

    unsigned long irq = arch_save_interrupts();
    spinlock_acquire(&process_lock);

    bool have_child = false;
    process_t* found = NULL;
    for (process_t* p = process_list_head; p; p = p->next) {
        if (p->ppid != parent->pid) continue;
        if (pid > 0 && (int)p->pid != pid) continue;
        have_child = true;
        if (p->zombie) { found = p; break; }
    }

    int ret = have_child ? 0 : -1;
    if (found) {
        process_t** pp = &process_list_head;
        while (*pp && *pp != found) pp = &(*pp)->next;
        if (*pp) *pp = found->next;
        ret = (int)found->pid;
        if (status_out) *status_out = found->exit_status;
    }

    spinlock_release(&process_lock);
    arch_restore_interrupts(irq);

    if (found) free(found);
    return ret;
}

process_t* process_get_current(void) {
    extern thread_t* scheduler_get_current_thread(void);
    thread_t* t = scheduler_get_current_thread();
    if (t) return t->process;
    return NULL;
}

static void process_register(process_t* proc) {
    spinlock_acquire(&process_lock);
    proc->next = process_list_head;
    process_list_head = proc;
    spinlock_release(&process_lock);
}

static void thread_stack_release(thread_stack_t* st) {
    if (!st) return;
    if (st->base) free(st->base);
    free(st);
}

process_t* process_clone(process_t* parent, thread_t* parent_thread, const struct registers* frame, const process_clone_opts_t* opts) {
    if (!parent || !parent_thread || !frame) return NULL;

    const uint64_t stack_lo = USER_STACK_TOP - USER_STACK_SIZE;

    process_t* child = (process_t*)malloc(sizeof(process_t));
    if (!child) return NULL;
    memset(child, 0, sizeof(process_t));

    child->page_table = clone_user_address_space(parent->page_table, stack_lo, USER_STACK_TOP, &child->owned_pages);
    if (!child->page_table) {
        printk(KMT_LOG, "Process", "clone: failed to copy the address space of pid %d", parent->pid);
        free(child);
        return NULL;
    }

    thread_stack_t* stack = (thread_stack_t*)malloc(sizeof(thread_stack_t));
    void* block = stack ? page_alloc(USER_STACK_SIZE / 0x1000) : NULL;
    if (!block) {
        if (stack) free(stack);
        goto fail_pt;
    }
    memset(block, 0, USER_STACK_SIZE);

    stack->base = block;
    stack->size = USER_STACK_SIZE;
    stack->user_mapped = true;

    for (uint64_t off = 0; off < USER_STACK_SIZE; off += 0x1000) {
        if (map_page_4k(child->page_table, stack_lo + off, hhdm_virt_to_phys((uint8_t*)block + off),
                        VMF_USER | VMF_WRITE) != 0) {
            thread_stack_release(stack);
            goto fail_pt;
        }
    }

    uint64_t user_sp = 0;
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    user_sp = frame->rsp;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    user_sp = frame->x[2];
#endif
    stack->ptr = (void*)user_sp;

    uint64_t copy_from = (user_sp >= 128) ? user_sp - 128 : 0;
    if (copy_from < stack_lo || copy_from > USER_STACK_TOP) copy_from = stack_lo;
    if (user_sp < stack_lo || user_sp > USER_STACK_TOP) copy_from = stack_lo; // odd rsp: be safe, copy all
    memcpy((uint8_t*)block + (copy_from - stack_lo), (void*)copy_from, USER_STACK_TOP - copy_from);

    spinlock_acquire(&process_lock);
    child->pid = next_pid++;
    spinlock_release(&process_lock);
    child->ppid = parent->pid;
    memcpy(child->name, parent->name, sizeof(child->name));
    child->brk_start     = parent->brk_start;
    child->brk_current   = parent->brk_current;
    child->mmap_start    = parent->mmap_start;
    child->mmap_current  = parent->mmap_current;
    child->syscall_conv  = parent->syscall_conv;
    memcpy(child->fds, parent->fds, sizeof(child->fds));

    uint64_t fs_base = parent_thread->fs_base;
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    fs_base = read_msr(IA32_FS_BASE);
#endif
    thread_t* t = thread_clone_user(parent_thread, frame, stack, fs_base);
    if (!t) {
        thread_stack_release(stack);
        goto fail_pt;
    }
    t->process = child;
    child->main_thread = t;
    child->main_tid = t->id;

    if (opts) {
        if (opts->child_stack) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
            t->regs->rsp = opts->child_stack;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
            t->regs->x[2] = opts->child_stack;
#endif
        }
        if (opts->set_tls) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
            t->fs_base = opts->tls;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
            t->regs->x[4] = opts->tls;
#endif
        }
        if (opts->child_tid_addr) {
            uint32_t* slot = (uint32_t*)vm_user_to_kernel(child->page_table, opts->child_tid_addr);
            if (slot) *slot = t->id;
        }
    }

    process_register(child);
    scheduler_enqueue(t);
    return child;

fail_pt:
    free_page_table(child->page_table);
    vm_page_list_free(&child->owned_pages);
    free(child);
    return NULL;
}

void process_replace(process_t* proc, thread_t* thread, struct registers* frame, struct elf_prepared* prepared, const char* name) {
    uint64_t* old_pt = proc->page_table;
    thread_stack_t* old_stack = thread->stacks.main;

    proc->page_table = prepared->l4;
    write_cr3(hhdm_virt_to_phys(prepared->l4));

    free_page_table(old_pt);
    vm_page_list_free(&proc->owned_pages);
    thread_stack_release(old_stack);

    proc->owned_pages = prepared->pages;
    thread->stacks.main = prepared->stack;
    prepared->l4 = NULL;
    prepared->stack = NULL;
    memset(&prepared->pages, 0, sizeof(prepared->pages));

    proc->brk_start     = prepared->brk_start;
    proc->brk_current   = prepared->brk_start;
    proc->mmap_start    = 0x400000000000;
    proc->mmap_current  = proc->mmap_start;

    if (name) {
        size_t i = 0;
        for (; name[i] && i < sizeof(proc->name) - 1; i++) proc->name[i] = name[i];
        proc->name[i] = '\0';
    }

    thread->fs_base = 0;
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    write_msr(IA32_FS_BASE, 0);

    memset(frame, 0, sizeof(*frame));
    frame->rip    = prepared->entry;
    frame->rsp    = prepared->sp;
    frame->rflags = 0x202;
    frame->cs     = 0x1b;
    frame->ss     = 0x23;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    uint64_t sstatus = frame->sstatus;
    uint64_t scause  = frame->scause;
    memset(frame, 0, sizeof(*frame));
    frame->sstatus = sstatus;
    frame->scause  = scause;
    frame->x[2]    = prepared->sp;
    frame->sepc    = prepared->entry - 4;
#endif
}
