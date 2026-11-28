#include <types.h>
#include <lib/screen.h>
#include <constants.h>
#include <arch/common.h>

extern bool context_switch_lock;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
extern void lapic_broadcast_nmi(void);
#endif

void panic(const char* reason, registers_t* regs) {
    static bool panicked = false;

    arch_cli();
    context_switch_lock = true;

    if (__atomic_exchange_n(&panicked, true, __ATOMIC_SEQ_CST)) {
        printk_remove_console_suspension();
        printk(KMT_ERROR, "Core", "\x1b[91mDouble kernel panic\x1b[0m: %s", reason);
        printk(KMT_ERROR, "Core", "  - A kernel panic occured while handling the first kernel panic.");
        printk(KMT_ERROR, "Core", "  - Please file a bug report: https://github.com/gorciu-official/Utopia/issues/new");
        printk(KMT_ERROR, "Core", "  - The system enters a halted state, please restart the computer manually");

        while (true) {
            arch_cli();
            arch_wfi();
        }
    } else
        panicked = true;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    lapic_broadcast_nmi();
#endif

    printk_remove_console_suspension();
    printk(KMT_ERROR, "Core", "\x1b[91mKernel panic\x1b[0m: %s", reason);
    printk(
        KMT_ERROR, "Core", "  On CPU %d; invoked by %s",
        current_processor_id(), regs == NULL ? "code" : "interrupt"
    );
    printk(KMT_ERROR, "Core", "  Printing architecture-specific backtrace:");

    if (regs != NULL) {
        arch_print_backtrace(regs);
    } else {
        printk(KMT_ERROR, "Core", "    - Backtrace currently not available on non-interrupt panics.");
    }

    printk(KMT_ERROR, "Core", "  End of backtrace.");
    printk(KMT_ERROR, "Core", "  If you believe this is a bug in Utopia, please file out a bug report:");
    printk(KMT_ERROR, "Core", "    - https://github.com/gorciu-official/Utopia/issues/new");
    printk(KMT_ERROR, "Core", "  The system enters a halted state, please restart your computer.");

    while (true) {
        arch_cli();
        arch_wfi();
    }

    __builtin_unreachable();
}
