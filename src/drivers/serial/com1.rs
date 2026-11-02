#![no_std]
#![cfg(target_arch = "x86_64")]

use core::arch::asm;

const COM1: u16 = 0x3F8;

#[inline]
unsafe fn arch_inb(port: u16) -> u8 {
    let result: u8;

    asm!(
        "in al, dx",
        in("dx") port,
        out("al") result,
        options(nomem, nostack, preserves_flags)
    );

    result
}

#[inline]
unsafe fn arch_outb(port: u16, value: u8) {
    asm!(
        "out dx, al",
        in("dx") port,
        in("al") value,
        options(nomem, nostack, preserves_flags)
    );
}

#[no_mangle]
pub extern "C" fn arch_init_serial() -> i32 {
	unsafe {
		arch_outb(COM1 + 1, 0x00);
		arch_outb(COM1 + 3, 0x80);
		arch_outb(COM1 + 0, 0x03);
		arch_outb(COM1 + 1, 0x00);
		arch_outb(COM1 + 3, 0x03);
		arch_outb(COM1 + 2, 0xC7);
		arch_outb(COM1 + 4, 0x0B);

		arch_outb(COM1 + 4, 0x1E);
		arch_outb(COM1 + 0, 0xAE);

		if arch_inb(COM1) != 0xAE {
			return 1;
		}

		arch_outb(COM1 + 4, 0x0F);
	}

	0
}

#[no_mangle]
pub extern "C" fn arch_serial_putchar(character: i8) {
	unsafe {
		while arch_inb(COM1 + 5) & 0x20 == 0 {}
		arch_outb(COM1, character as u8);
	}
}
