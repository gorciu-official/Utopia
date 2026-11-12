#![cfg(target_arch = "riscv64")]

const UART_BASE:                 usize     = 0x1000_0000;
const UART_RBR:                  usize     = 0;
const UART_THR:                  usize     = 0;
const UART_IER:                  usize     = 1;
const UART_IIR:                  usize     = 2;
const UART_IER_RX:               u8        = 1 << 0;
const UART_LSR:                  usize     = 5;
const UART_READY:                u8        = 1 << 5;
const UART_DATA_READY:           u8        = 1 << 0;
const UART_IIR_NO_INTERRUPT:     u8        = 1 << 0;

use core::ffi::c_char;

use crate::kernel::ffi::{KeyboardKeyPress, KeyboardKeyPressKind, keyboard_handle_keypress, phys_to_virt};

unsafe fn enable_interrupts() {
    unsafe {
        let uart = phys_to_virt(UART_BASE) as *mut u8;

        uart.add(UART_IER).write_volatile(UART_IER_RX);
    }
}

#[unsafe(no_mangle)]
extern "C" fn arch_serial_putchar(c: i8) {
    let virt_addr: usize = unsafe { phys_to_virt(UART_BASE) };
    let uart = virt_addr as *mut u8;

    while (unsafe { uart.add(UART_LSR).read_volatile() } & UART_READY) == 0 {}

    unsafe { uart.add(UART_THR).write_volatile(c as u8) };
}

#[unsafe(no_mangle)]
pub extern "C" fn arch_uart_handler() {
    let virt_addr = unsafe { phys_to_virt(UART_BASE) };
    let uart = virt_addr as *mut u8;

    unsafe {
        let iir = uart.add(UART_IIR).read_volatile();

        if iir & UART_IIR_NO_INTERRUPT == 0 {
            let lsr = uart.add(UART_LSR).read_volatile();

            if lsr & UART_DATA_READY != 0 {
                let character = uart.add(UART_RBR).read_volatile();

                let keypress = KeyboardKeyPress {
                    kind: KeyboardKeyPressKind::Char,
                    char: character as c_char,
                };

                keyboard_handle_keypress(keypress);
            }
        }
    }
}

#[unsafe(no_mangle)]
extern "C" fn arch_init_serial() -> isize {
    // limine has a habit of fucking up serial for some reason so let's 
    // clear the script with ANSI magic ;)
    let clear_seq: &str = "\x1b[2J\x1b[3J\x1b[H";
    for c in clear_seq.bytes() {
        arch_serial_putchar(c as i8);
    }

    unsafe {
        enable_interrupts();
    }

    0 // idk why but it was returing 0 in the original code so i just kept it for compatibility
}
