#include <types.h>
#include <arch/x86_64/common.h>
#include <arch/x86_64/pmio.h>
#include <lib/screen.h>

bool has_invariant_tsc() {
    unsigned int eax, ebx, ecx, edx;

    __cpuid(0x80000000, eax, ebx, ecx, edx);

    if (eax < 0x80000007)
        return false;

    __cpuid(0x80000007, eax, ebx, ecx, edx);

    return !!(edx & (1 << 8));
}

uint64_t invariant_tsc_to_ns(uint64_t ticks, uint64_t tsc_hz) {
    // gorciu: this is generally a peak anti-overflow function, it could be done
    // gorciu: far simpler, but we do not have 128-bit cpus yet... so sad so sad...
    // gorciu: intel please make a 128-bit cpu. long long long long mode 🗣️🔥🔥🔥🔥
    // gorciu: (c types reference). oh wait amd is better. amd do it. you will now
    // gorciu: be better than intel.
    //   AMD joined the chat
    // amd: but i am already better than intel, has anyone ever pucharsed an Intel
    // amd: CPU after 2020?
    //   Intel joined the chat
    // intel: yes! we have our peak intel core ultra or something with revolutionary
    // intel: AI thechnology!
    // amd: who cares about your AI intel? gamers buy my CPUs, not yours
    // intel: i am innovating the world
    // amd: okay i am gonna innovate with introducing 128-bit CPUs
    // intel: fuck its gonna be AMD128 like AMD64 and not Intel128...
    // amd: u just had to focus on what's important, ai is certainly not
    //   Sam Altman joined the chat
    // altman: ai is the future of humanity. with our newest cybersecurity
    // altman: model GPT 6.4...
    //   Gorciu, AMD and Intel left the chat at the same moment
    // altman: why am i alone
    // altman: kurwa przyjdź ktoś, oddam wam ten ram
    uint64_t sec = ticks / tsc_hz;
    uint64_t rem = ticks % tsc_hz;
    return sec * 1000000000ULL + rem * 1000000000ULL / tsc_hz;
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
    const uint32_t pit_hz = 1193182;

    if (delay_ms > 50)
        delay_ms = 50;

    uint32_t count = (uint32_t)((uint64_t)pit_hz * delay_ms / 1000);

    arch_outb(0x61, (arch_inb(0x61) & 0xFC) | 0x01);
    arch_outb(0x43, 0xB0);
    arch_outb(0x42, (uint8_t)(count & 0xFF));
    arch_outb(0x42, (uint8_t)((count >> 8) & 0xFF));

    uint64_t start = read_tsc();
    while (!(arch_inb(0x61) & 0x20))
        continue;
    uint64_t end = read_tsc();

    uint64_t calibval = (end - start) * 1000ULL / delay_ms;

    printk(KMT_LOG, "Clock", "Invariant TSC clock source calibrated to %p Hz", calibval);

    return calibval;
}

static uint64_t tsc_frequency = 0;
static uint64_t tsc_boot_value = 0;
static bool tsc_initialized = false;

void tsc_init() {
    if (!has_invariant_tsc()) {
        return printk(KMT_LOG, "Clock", "Warning: invariant TSC not present, clock will return 0. PIT is no longer a clock source.");
    }

    tsc_frequency = calibrate_tsc(50);

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
