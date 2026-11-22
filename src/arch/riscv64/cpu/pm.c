#include <kernel/panic.h>

#include "sbi.h"

void arch_pm_do_reboot() {
    sbicall(
        SBI_EXT_SRST,
        SBI_EXT_SRST_RESET,
        SBI_SRST_RESET_TYPE_COLD,
        SBI_SRST_RESET_REASON_NONE
    );

    panic("Unsuccessfull system reboot attempt", NULL);
}
