#include <types.h>
#include <arch/x86_64/common.h>

extern void syscall_entry();

void init_syscall() {
    uint64_t efer = read_msr(0xC0000080);
    efer |= 1; 
    write_msr(0xC0000080, efer);
    
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x1b << 48);
    write_msr(0xC0000081, star);

    write_msr(0xC0000082, (uint64_t)syscall_entry);

    write_msr(0xC0000084, (1 << 9) | (1 << 10)); 
}
