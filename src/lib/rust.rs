extern crate alloc;

use core::{
    ffi::{c_char, c_void}, 
    ptr, panic::PanicInfo
};
use core::alloc::{
    Layout, GlobalAlloc
};

unsafe extern "C" {
    fn panic(msg: *const c_char, int_frame: *const c_void) -> i32;
    fn malloc(size: usize) -> *mut c_void;
    fn free(ptr: *mut c_void) -> ();
}

struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        malloc(layout.size()) as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        free(ptr as *mut c_void);
    }
}

#[global_allocator]
static ALLOCATOR: KernelAllocator = KernelAllocator;

#[panic_handler]
fn rust_panic(_info: &PanicInfo) -> ! {
unsafe {
    panic(
        "Reached panic path in Rust!\0".as_ptr() as *const i8, ptr::null()
    );

    // to satisfy rust's compiler, not gonna run anyways
    loop {
        core::hint::spin_loop();
    }
}
}
