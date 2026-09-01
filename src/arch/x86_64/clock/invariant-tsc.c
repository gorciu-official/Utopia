#include <types.h>
#include <arch/x86_64/common.h>
#include <lib/screen.h>
#include <panic.h>

bool has_invariant_tsc() {
    unsigned int eax, ebx, ecx, edx;

    __cpuid(0x80000000, eax, ebx, ecx, edx);

    if (eax < 0x80000007)
        return false;

    __cpuid(0x80000007, eax, ebx, ecx, edx);

    return !!(edx & (1 << 8));
}

uint64_t invariant_tsc_to_ns(uint64_t ticks, uint64_t tsc_hz) {
    return ticks * 1000000000ULL / tsc_hz;
}

static inline uint64_t read_tsc() {
    unsigned int lo, hi;

    __asm__ volatile (
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );

    return ((uint64_t)hi << 32) | lo;
}

uint64_t tsc_delta_ns(uint64_t start, uint64_t end, uint64_t tsc_hz) {
    uint64_t ticks = end - start;
    return invariant_tsc_to_ns(ticks, tsc_hz);
}

uint64_t calibrate_tsc(uint64_t delay_ms) {
    asm volatile("hlt");
    uint64_t start = read_tsc();

    asm volatile("hlt");
    uint64_t end = read_tsc();

    // ^^^ 
    // explanation: tsc_init and therefore calibrate_tsc is called after 
    //              initializing PIT clock event, so PIT interrupts should now
    //              work. the first halt is used for more accurate results
    //              (since we are all of the time minus the time to 
    //              process the interrupt iand not who knows how much), the 
    //              second to actually calibrate tsc

    uint64_t ticks = end - start;
    uint64_t calibval =  (ticks * 1000ULL) / delay_ms;

    printk("Clock", "Invariant TSC clock source calibrated to %p Hz", calibval); 

    return calibval;
}

static uint64_t tsc_frequency = 0;
static uint64_t tsc_boot_value = 0;
static bool tsc_initialized = false;

void tsc_init() {
    if (!has_invariant_tsc()) {
        return printk("Clock", "Warning: invariant TSC not present, clock will return 0. PIT is no longer a clock source.");
    }

    tsc_frequency = calibrate_tsc(10);

    tsc_boot_value = read_tsc();

    tsc_initialized = true;
}

uint64_t tsc_get_ns_time() {
    if (!has_invariant_tsc() || !tsc_initialized)
        return 0;

    uint64_t now = read_tsc();

    return invariant_tsc_to_ns(
        now - tsc_boot_value,
        tsc_frequency
    );
}
