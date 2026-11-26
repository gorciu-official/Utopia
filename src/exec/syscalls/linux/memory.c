#include <config.h>

#include <fs/filesystem.h>

#include <lib/screen.h>

#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/user.h>

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

#define MM_USER_LIMIT     (1ULL << USER_VA_BITS)
#define MM_MIN_ADDR       0x10000ULL
#define MM_MMAP_LIMIT     (USER_STACK_TOP - USER_STACK_SIZE)
#define MM_PAGE           0x1000ULL
#define MM_MAX_PLACEMENT_TRIES 64

static bool mm_range_is_unmapped(process_t* process, uint64_t va, uint64_t pages) {
    for (uint64_t i = 0; i < pages; i++) {
        if (vm_user_to_kernel(process->page_table, va + i * MM_PAGE))
            return false;
    }
    return true;
}

static void mm_release_range(process_t* process, uint64_t va, uint64_t pages) {
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t addr = va + i * MM_PAGE;
        void* kpage = vm_user_to_kernel(process->page_table, addr);
        if (!kpage) continue;

        unmap_page_4k(process->page_table, addr);
        if (vm_page_list_remove(&process->owned_pages, kpage))
            free(kpage);
    }
}

static int mm_populate_range(process_t* process, uint64_t va, uint64_t pages,
                             vm_flags_t pflags, vnode_t* node, uint64_t offset) {
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t addr = va + i * MM_PAGE;

        uint64_t* page = pt_pool_alloc();
        if (!page) goto oom;

        if (node) {
            uint64_t file_off = offset + i * MM_PAGE;
            if (file_off < node->size) {
                uint64_t want = node->size - file_off;
                if (want > MM_PAGE) want = MM_PAGE;

                uint64_t got = 0;
                if (node->ops->read(node, page, want, file_off, &got) != 0) {
                    free(page);
                    mm_release_range(process, va, i);
                    return -EIO;
                }
            }
        }

        if (vm_page_list_add(&process->owned_pages, page) != 0) {
            free(page);
            goto oom;
        }

        if (map_page_4k(process->page_table, addr, hhdm_virt_to_phys(page), pflags) != 0) {
            vm_page_list_remove(&process->owned_pages, page);
            free(page);
            goto oom;
        }
    }
    return 0;

oom:
    mm_release_range(process, va, pages);
    return -ENOMEM;
}

static int mm_pick_address(process_t* process, uint64_t hint, uint64_t flags,
                           uint64_t size, uint64_t* out, bool* bump) {
    uint64_t pages = size / MM_PAGE;
    *bump = false;

    if (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) {
        if (hint & (MM_PAGE - 1)) return -EINVAL;
        if (hint < MM_MIN_ADDR || !user_range_ok(hint, size)) return -ENOMEM;
        if ((flags & MAP_FIXED_NOREPLACE) && !mm_range_is_unmapped(process, hint, pages))
            return -EEXIST;
        *out = hint;
        return 0;
    }

    if (hint) {
        uint64_t aligned = (hint + (MM_PAGE - 1)) & ~(MM_PAGE - 1);
        if (aligned >= hint && aligned >= MM_MIN_ADDR && aligned < MM_MMAP_LIMIT &&
            size <= MM_MMAP_LIMIT - aligned &&
            mm_range_is_unmapped(process, aligned, pages)) {
            *out = aligned;
            return 0;
        }
    }

    uint64_t cur = process->mmap_current;
    for (int attempt = 0; attempt < MM_MAX_PLACEMENT_TRIES; attempt++) {
        if (cur < MM_MIN_ADDR || size > MM_MMAP_LIMIT || cur > MM_MMAP_LIMIT - size)
            return -ENOMEM;
        if (mm_range_is_unmapped(process, cur, pages)) {
            *out = cur;
            *bump = true;
            return 0;
        }
        cur += size;
    }
    return -ENOMEM;
}

SYSCALL_DEFINE_LINUX(mmap) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;
    uint64_t flags  = regs->arg4;
    int      fd     = (int)regs->arg5;
    uint64_t offset = regs->arg6;

    if (length == 0) return SCRC(-EINVAL);
    if (prot & ~(uint64_t)(PROT_READ | PROT_WRITE | PROT_EXEC)) return SCRC(-EINVAL);

    uint64_t type = flags & MAP_TYPE_MASK;
    if (type != MAP_SHARED && type != MAP_PRIVATE && type != 3 /* SHARED_VALIDATE */)
        return SCRC(-EINVAL);

    if (length > MM_USER_LIMIT) return SCRC(-ENOMEM);
    uint64_t alloc_size = (length + (MM_PAGE - 1)) & ~(MM_PAGE - 1);
    uint64_t pages = alloc_size / MM_PAGE;

    vnode_t* node = NULL;
    if (!(flags & MAP_ANONYMOUS)) {
        if (offset & (MM_PAGE - 1)) return SCRC(-EINVAL);
        if (offset > ~0ULL - alloc_size) return SCRC(-EINVAL);
        if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used || !process->fds[fd].vnode)
            return SCRC(-EBADF);

        node = process->fds[fd].vnode;
        if (node->type == VNODE_TYPE_DIR || !node->ops || !node->ops->read)
            return SCRC(-ENODEV);
    }

    uint64_t target;
    bool bump;
    int err = mm_pick_address(process, addr, flags, alloc_size, &target, &bump);
    if (err) return SCRC(err);

    dprintk("mmap", "addr=%p len=%p prot=%p flags=%p fd=%d offset=%p -> %p",
        addr, length, prot, flags, fd, offset, target);

    vm_flags_t pflags = VMF_USER;
    if (prot & PROT_WRITE) pflags |= VMF_WRITE;
    if (prot & PROT_EXEC)  pflags |= VMF_EXEC;

    if (flags & MAP_FIXED)
        mm_release_range(process, target, pages);

    err = mm_populate_range(process, target, pages, pflags, node, offset);
    if (err) return SCRC(err);

    if (bump) process->mmap_current = target + alloc_size;
    return SCRC(target);
}

SYSCALL_DEFINE_LINUX(mprotect) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;

    dprintk("Syscall", "mprotect called with addr=%p length=%p prot=%p", addr, length, prot);

    if (addr & (MM_PAGE - 1)) return SCRC(-EINVAL);
    if (prot & ~(uint64_t)(PROT_READ | PROT_WRITE | PROT_EXEC)) return SCRC(-EINVAL);
    if (length == 0) return SCRC(0);
    if (length > MM_USER_LIMIT) return SCRC(-ENOMEM);

    uint64_t aligned_len = (length + (MM_PAGE - 1)) & ~(MM_PAGE - 1);
    if (!user_range_ok(addr, aligned_len)) return SCRC(-ENOMEM);
    uint64_t pages = aligned_len / MM_PAGE;

    for (uint64_t i = 0; i < pages; i++) {
        if (!vm_user_to_kernel(process->page_table, addr + i * MM_PAGE))
            return SCRC(-ENOMEM);
    }

    vm_flags_t pflags = VMF_USER;
    if (prot & PROT_WRITE) pflags |= VMF_WRITE;
    if (prot & PROT_EXEC)  pflags |= VMF_EXEC;

    for (uint64_t i = 0; i < pages; i++) {
        if (protect_page_4k(process->page_table, addr + i * MM_PAGE, pflags) != 0)
            return SCRC(-ENOMEM);
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(munmap) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;

    dprintk("Syscall", "munmap called with addr=%p length=%p", addr, length);

    if (addr & (MM_PAGE - 1)) return SCRC(-EINVAL);
    if (length == 0 || length > MM_USER_LIMIT) return SCRC(-EINVAL);

    uint64_t aligned_len = (length + (MM_PAGE - 1)) & ~(MM_PAGE - 1);
    if (!user_range_ok(addr, aligned_len)) return SCRC(-EINVAL);

    mm_release_range(process, addr, aligned_len / MM_PAGE);
    return SCRC(0);
}

#endif
