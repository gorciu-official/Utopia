#include <process.h>
#include <memory.h>
#include <arch/x86_64/common.h>
#include <lib/screen.h>
#include <lib/string.h>
#include <lib/spinlock.h>

static spinlock_t process_lock;
static process_t* process_list_head = NULL;
static uint32_t next_pid = 1; 

void process_init(void) {
    spinlock_init(&process_lock);
    process_list_head = NULL;
}

process_t* process_create(const char* name, void (*entry_point)(void*), void* arg, int ring) {
    process_t* proc = (process_t*)malloc(sizeof(process_t));
    if (!proc) {
        printk("Process", "Failed to allocate PCB for '%s'!", name);
        return NULL;
    }

    uint64_t* pt = clone_page_table();
    if (!pt) {
        printk("Process", "Failed to clone page table for '%s'!", name);
        free(proc);
        return NULL;
    }

    proc->pid = next_pid++;
    strcpy(proc->name, name);
    proc->page_table = pt;

    thread_t* main_thread = thread_create(name, entry_point, arg, ring);
    if (!main_thread) {
        printk("Process", "Failed to create main thread for '%s'!", name);
        free_page_table(pt);
        free(proc);
        return NULL;
    }
    main_thread->process = proc;
    proc->main_thread = main_thread;

    spinlock_acquire(&process_lock);
    proc->next = process_list_head;
    process_list_head = proc;
    spinlock_release(&process_lock);

    return proc;
}

void process_terminate(process_t* proc) {
    if (!proc) return;

    if (proc->main_thread) {
        proc->main_thread->state = THREAD_STATE_TERMINATED;
        proc->main_thread->process = NULL; 
    }

    if (proc->page_table) {
        free_page_table(proc->page_table);
        proc->page_table = NULL;
    }

    spinlock_acquire(&process_lock);
    if (process_list_head == proc) {
        process_list_head = proc->next;
    } else {
        process_t* prev = process_list_head;
        while (prev && prev->next != proc) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = proc->next;
        }
    }
    spinlock_release(&process_lock);

    free(proc);
}

process_t* process_get_current(void) {
    extern thread_t* scheduler_get_current_thread(void);
    thread_t* t = scheduler_get_current_thread();
    if (t) return t->process;
    return NULL;
}
