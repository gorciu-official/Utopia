#include <types.h>
#include <arch/x86_64/msr.h>

extern void syscall_entry();

void init_syscall() {
    uint64_t efer = read_msr(IA32_EFER);
    efer |= 1; 
    write_msr(IA32_EFER, efer);
    
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x1b << 48);
    write_msr(IA32_STAR, star);

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);

    write_msr(IA32_FMASK, (1 << 9) | (1 << 10)); 
}
