#![no_std]
#![no_main]

// Those are set by uconfig and are 100% expected behaviour, so we would rather
// want `cargo` and `rustc` to stop yapping.
#![allow(unexpected_cfgs)]

// Only God knows why we need to do that.
extern crate alloc;

mod arch;
mod drivers;
mod kernel;
