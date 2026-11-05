#![cfg(target_arch = "x86_64")]

const COM1: u16 = 0x3F8;

use crate::arch::pmio::{arch_inb, arch_outb};

#[unsafe(no_mangle)]
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

#[unsafe(no_mangle)]
pub extern "C" fn arch_serial_putchar(character: i8) {
unsafe {
	while arch_inb(COM1 + 5) & 0x20 == 0 {}
	arch_outb(COM1, character as u8);
}
}
