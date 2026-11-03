use core::{
    ptr, panic::PanicInfo,
    ffi::{ c_char, c_void }
};

unsafe extern "C" {
    fn panic(msg: *const c_char, int_frame: *const c_void) -> i32;
}

#[panic_handler]
fn rust_panic(_info: &PanicInfo) -> ! {
unsafe {
    panic(
        "Reached panic path in Rust!\0".as_ptr() as *const c_char, ptr::null()
    );

    // to satisfy rust's compiler, not gonna run anyways
    loop {
        core::hint::spin_loop();
    }
}
}
