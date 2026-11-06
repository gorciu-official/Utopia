use core::ffi::{c_void, c_char};

unsafe extern "C" {
    #[allow(dead_code)] // remove later
    pub fn phys_to_virt(phys: usize) -> usize;
    pub fn malloc(size: usize) -> *mut c_void;
    pub fn free(ptr: *mut c_void) -> ();
    pub fn panic(msg: *const c_char, int_frame: *const c_void) -> i32;
}
