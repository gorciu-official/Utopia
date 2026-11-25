use core::{ffi::c_char, panic::PanicInfo, ptr};
use alloc::string::ToString;

use crate::kernel::ffi::panic;

#[panic_handler]
fn rust_panic(info: &PanicInfo) -> ! {
    let message = (
        "Rust panic: ".to_string()
            + info.message()
                .as_str()
                .unwrap_or("Unknown message")
    ) + "\0";

    unsafe {
        panic(
            message.as_ptr() as *const c_char,
            ptr::null(),
        );
    }

    loop {
        core::hint::spin_loop();
    }
}
