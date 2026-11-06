#![cfg(target_arch = "x86_64")]

use core::arch::asm;

#[unsafe(no_mangle)]
pub unsafe fn arch_inb(port: u16) -> u8 {
    let result: u8;

    unsafe {
        asm!(
            "in al, dx",
            in("dx") port,
            out("al") result,
            options(nomem, nostack, preserves_flags)
        );
    }

    result
}

#[unsafe(no_mangle)]
pub unsafe fn arch_outb(port: u16, value: u8) {
    unsafe {
        asm!(
            "out dx, al",
            in("dx") port,
            in("al") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

#[unsafe(no_mangle)]
pub unsafe fn arch_inw(port: u16) -> u16 {
    let result: u16;

    unsafe {
        asm!(
            "in ax, dx",
            in("dx") port,
            out("ax") result,
            options(nomem, nostack, preserves_flags)
        );
    }

    result
}

#[unsafe(no_mangle)]
pub unsafe fn arch_outw(port: u16, value: u16) {
    unsafe {
        asm!(
            "out dx, ax",
            in("dx") port,
            in("ax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}

#[unsafe(no_mangle)]
pub unsafe fn arch_inl(port: u16) -> u32 {
    let result: u32;

    unsafe {
        asm!(
            "in eax, dx",
            in("dx") port,
            out("eax") result,
            options(nomem, nostack, preserves_flags)
        );
    }

    result
}

#[unsafe(no_mangle)]
pub unsafe fn arch_outl(port: u16, value: u32) {
    unsafe {
        asm!(
            "out dx, eax",
            in("dx") port,
            in("eax") value,
            options(nomem, nostack, preserves_flags)
        );
    }
}
