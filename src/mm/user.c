#include <types.h>
#include <arch/memory.h>
#include <exec/process.h>
#include <mm/pmm.h>
#include <mm/user.h>

#define UACCESS_USER_LIMIT (1ULL << USER_VA_BITS)

bool user_range_ok(uint64_t addr, size_t len) {
    if (len == 0) return true;
    if (addr >= UACCESS_USER_LIMIT) return false;
    if (len > UACCESS_USER_LIMIT - addr) return false;
    return true;
}

// user address -> hddm
static uint8_t* uaccess_resolve(uint64_t* l4, uint64_t va, bool write) {
    uint64_t* table = l4;
    for (int level = PT_TOP_LEVEL; level >= 0; level--) {
        pte_t entry = table[pt_index(va, level)];
        if (!arch_pte_present(entry)) return NULL;

        if (level == 0 || arch_pte_is_leaf(entry, level)) {
            if (!arch_pte_user(entry)) return NULL;
            if (write && !arch_pte_writable(entry)) return NULL;
            uint64_t span = 1ULL << (12 + level * 9);
            return (uint8_t*)phys_to_virt(arch_pte_phys(entry)) + (va & (span - 1));
        }
        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }
    return NULL;
}

static inline size_t uaccess_page_left(uint64_t va) {
    return 0x1000 - (size_t)(va & 0xFFF);
}

// ---
// oh here we have the legendary functions

int copy_from_user(process_t* proc, void* kdst, const void* usrc, size_t len) {
    if (len == 0) return 0;
    if (!proc || !proc->page_table || !kdst)
        return -UACCESS_EFAULT;

    uint64_t va = (uint64_t)usrc;
    if (!user_range_ok(va, len))
        return -UACCESS_EFAULT;

    uint8_t* dst = (uint8_t*)kdst;
    while (len > 0) {
        uint8_t* src = uaccess_resolve(proc->page_table, va, false);
        if (!src) return -UACCESS_EFAULT;

        size_t chunk = uaccess_page_left(va);
        if (chunk > len) chunk = len;

        memcpy(dst, src, chunk);
        dst += chunk;
        va  += chunk;
        len -= chunk;
    }
    return 0;
}

int copy_to_user(process_t* proc, void* udst, const void* ksrc, size_t len) {
    if (len == 0) return 0;
    if (!proc || !proc->page_table || !ksrc)
        return -UACCESS_EFAULT;

    uint64_t va = (uint64_t)udst;
    if (!user_range_ok(va, len))
        return -UACCESS_EFAULT;

    const uint8_t* src = (const uint8_t*)ksrc;
    while (len > 0) {
        uint8_t* dst = uaccess_resolve(proc->page_table, va, true);
        if (!dst) return -UACCESS_EFAULT;

        size_t chunk = uaccess_page_left(va);
        if (chunk > len) chunk = len;

        memcpy(dst, src, chunk);
        src += chunk;
        va  += chunk;
        len -= chunk;
    }
    return 0;
}

int strncpy_from_user(process_t* proc, char* kdst, const char* usrc, size_t max) {
    if (max == 0 || !kdst) return -UACCESS_EFAULT;
    if (!proc || !proc->page_table) return -UACCESS_EFAULT;

    uint64_t va = (uint64_t)usrc;
    size_t done = 0;

    while (done < max) {
        if (!user_range_ok(va, 1)) return -UACCESS_EFAULT;

        uint8_t* src = uaccess_resolve(proc->page_table, va, false);
        if (!src) return -UACCESS_EFAULT;

        size_t chunk = uaccess_page_left(va);
        if (chunk > max - done) chunk = max - done;

        for (size_t i = 0; i < chunk; i++) {
            kdst[done + i] = (char)src[i];
            if (src[i] == '\0') return (int)(done + i + 1);
        }
        done += chunk;
        va   += chunk;
    }

    kdst[max - 1] = '\0';
    return -UACCESS_ENAMETOOLONG;
}
