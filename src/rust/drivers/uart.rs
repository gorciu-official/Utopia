#![cfg(target_arch = "riscv64")]

const UART_BASE: usize = 0x1000_0000;
const UART_THR: usize = 0;
const UART_LSR: usize = 5;
const UART_READY: u8 = 1 << 5;

use crate::krnl::ffi::phys_to_virt;

#[unsafe(no_mangle)]
extern "C" fn arch_serial_putchar(c: i8) {
    let virt_addr: usize = unsafe { phys_to_virt(UART_BASE) };
    let uart = virt_addr as *mut u8;

    while (unsafe { uart.add(UART_LSR).read_volatile() } & UART_READY) == 0 {}

    unsafe { uart.add(UART_THR).write_volatile(c as u8) };
}

#[unsafe(no_mangle)]
extern "C" fn arch_init_serial() -> isize {
    let clear_seq: &str = "\x1b[2J\x1b[3J\x1b[H";
    for c in clear_seq.bytes() {
        arch_serial_putchar(c as i8);
    }
    0 // idk why but it was returing 0 in the original code so i just kept it for compatibility
}
