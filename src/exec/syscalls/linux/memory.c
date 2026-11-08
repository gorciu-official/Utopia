#include <config.h>

#include <fs/filesystem.h>

#include <lib/screen.h>

#include <mm/pmm.h>
#include <mm/vmm.h>

#include "../common.h"
#include "common.h"

#ifdef CONFIG_SYSCALL_ABI_LINUX

static uint64_t page_align_up(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int grow_process_brk(process_t* process, uint64_t new_break) {
    uint64_t old_end = page_align_up(process->brk_current);
    uint64_t new_end = page_align_up(new_break);

    for (uint64_t addr = old_end; addr < new_end; addr += 0x1000) {
        uint64_t* page = pt_pool_alloc();
        if (!page) return -1;
        uint64_t phys = hhdm_virt_to_phys(page);

        if (map_page_4k(process->page_table, addr, phys, VMF_USER | VMF_WRITE) != 0) {
            free(page);
            return -1;
        }
        // released when the process exits or execs
        if (vm_page_list_add(&process->owned_pages, page) != 0) return -1;
    }

    return 0;
}

SYSCALL_DEFINE_LINUX(brk) {
    (void)thread;

    uint64_t requested_break = regs->arg1;

    if (!process) {
        return SCRC(-ENOSYS);
    }

    if (requested_break == 0) {
        return SCRC(process->brk_current);
    }

    if (requested_break < process->brk_start || requested_break >= USER_HEAP_MAX) {
        return SCRC(process->brk_current);
    }

    if (requested_break > process->brk_current && grow_process_brk(process, requested_break) != 0) {
        return SCRC(process->brk_current);
    }

    process->brk_current = requested_break;
    return SCRC(process->brk_current);
}

SYSCALL_DEFINE_LINUX(mmap) {
    (void)thread;
    if (!process) return SCRC(-1);
    
    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;
    uint64_t flags  = regs->arg4;
    int64_t  fd     = (int64_t)regs->arg5;
    uint64_t offset = regs->arg6;

    if (length == 0) return SCRC(0);

    uint64_t alloc_size = page_align_up(length);
    uint64_t pages_count = alloc_size / 0x1000;

    uint64_t target_vaddr;
    if (addr == 0) {
        target_vaddr = process->mmap_current;
        process->mmap_current += alloc_size;
    } else {
        target_vaddr = page_align_up(addr);
    }

    dprintk("mmap", "addr=%p len=%p prot=%p flags=%p fd=%d offset=%p -> %p",
        addr, length, prot, flags, (int)fd, offset, target_vaddr);

    uint64_t pflags = VMF_USER;
    if (prot & 0x2) pflags |= VMF_WRITE;
    if (prot & 0x1) pflags |= VMF_EXEC;

    bool anonymous = (flags & MAP_ANONYMOUS) || fd < 0;
    vnode_t *node = NULL;
    if (!anonymous) {
        if (fd < 0 || !process->fds[fd].vnode) return SCRC(-EBADF); 
        node = process->fds[fd].vnode;
    }

    void *virt_block = page_alloc(pages_count);
    if (!virt_block) return SCRC(0);

    uint64_t phys_base = hhdm_virt_to_phys(virt_block);
    memset(virt_block, 0, alloc_size);

    if (node) {
        if (offset < node->size) {
            uint64_t want = node->size - offset;
            if (want > alloc_size) want = alloc_size;
            
            uint64_t bytes_read = 0;
            node->ops->read(node, virt_block, want, offset, &bytes_read);
        }
    }

    for (uint64_t i = 0; i < pages_count; i++) {
        uint64_t vaddr = target_vaddr + (i * 0x1000);
        uint64_t phys  = phys_base + (i * 0x1000);

        if (map_page_4k(process->page_table, vaddr, phys, pflags) != 0) {
            free(virt_block); 
            return SCRC(0);
        }
    }

    return SCRC(target_vaddr);
}

SYSCALL_DEFINE_LINUX(mprotect) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);
    
    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;
    
    dprintk("Syscall", "mprotect called with addr=%p length=%p prot=%p", addr, length, prot);

    if (length == 0) return SCRC(0);
    if (addr & 0xFFF) return SCRC(-EINVAL); 

    uint64_t aligned_len = page_align_up(length);

    uint64_t pflags = VMF_USER;
    if (prot & 0x2) pflags |= VMF_WRITE;
    if (prot & 0x4) pflags |= VMF_EXEC;

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (protect_page_4k(process->page_table, va, pflags) != 0) {
            return SCRC(-ENOMEM);
        }
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(munmap) {
    (void)thread;
    if (!process) return SCRC(-EINVAL); 

    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    
    dprintk("Syscall", "munmap called with addr=%p length=%p", addr, length);

    if (length == 0) return SCRC(-EINVAL);
    if (addr & 0xFFF) return SCRC(-EINVAL); 

    uint64_t aligned_len = page_align_up(length);

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (unmap_page_4k(process->page_table, va) != 0) {
            return SCRC(-12);
        }
    }

    return SCRC(0);
}

#endif 
