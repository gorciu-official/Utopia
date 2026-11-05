use core::ffi::c_void;
use core::alloc::{
    Layout, GlobalAlloc
};

use crate::krnl::ffi::{malloc, free};

struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
    unsafe {
        malloc(layout.size()) as *mut u8
    }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
    unsafe {
        free(ptr as *mut c_void);
    }
    }
}

#[global_allocator]
static ALLOCATOR: KernelAllocator = KernelAllocator;
