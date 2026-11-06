use core::{ffi::c_char, panic::PanicInfo, ptr};

use crate::krnl::ffi::panic;

#[panic_handler]
fn rust_panic(info: &PanicInfo) -> ! {
    unsafe {
        panic(
            info.message()
                .as_str()
                .unwrap_or("Unknown Rust code error")
                .as_ptr() as *const c_char,
            ptr::null(),
        );
    }

    // to satisfy rust's compiler, not gonna run anyways
    loop {
        core::hint::spin_loop();
    }
}
