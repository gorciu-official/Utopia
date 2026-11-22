#include <lib/string.h>

#include "../common.h"
#include "common.h"

static char linux_hostname[65] = "utopia-pc";

SYSCALL_DEFINE_LINUX(uname) {
    (void)process; (void)thread;

    struct utsname* ptr = (struct utsname*)regs->arg1;

    strcpy(ptr->sysname, "Utopia");
    strcpy(ptr->nodename, linux_hostname);
    strcpy(ptr->release, UTOPIA_VERSION);
    strcpy(ptr->version, "(no build information)");
    strcpy(ptr->machine, "x86_64");
    strcpy(ptr->domainname, "");

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(sethostname) {
    (void)process; (void)thread;

    const char* name = (const char*)regs->arg1;
    size_t len = (size_t)regs->arg2;

    if (len > 64)
        return SCRC(-EINVAL);
    if (len && !name)
        return SCRC(-EFAULT);

    if (len)
        memcpy(linux_hostname, name, len);
    linux_hostname[len] = '\0';

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(clock_gettime) {
    (void)process; (void)thread;

    int64_t clock_id = (int64_t)regs->arg1;
    struct linux_timespec* ts = (struct linux_timespec*)regs->arg2;

    if (!ts || (uint64_t)(uintptr_t)ts >= USER_VA_LIMIT ||
        (uint64_t)(uintptr_t)(ts + 1) >= USER_VA_LIMIT)
        return SCRC(-EFAULT);

    switch (clock_id) {
        // peak
        case LINUX_CLOCK_REALTIME:
        case LINUX_CLOCK_REALTIME_COARSE:
        case LINUX_CLOCK_MONOTONIC:
        case LINUX_CLOCK_MONOTONIC_RAW:
        case LINUX_CLOCK_MONOTONIC_COARSE:
        case LINUX_CLOCK_BOOTTIME:
        case LINUX_CLOCK_PROCESS_CPUTIME_ID:
        case LINUX_CLOCK_THREAD_CPUTIME_ID:
            break;
        default:
            return SCRC(-EINVAL);
    }

    uint64_t ns = arch_get_ns_time();
    ts->tv_sec  = (int64_t)(ns / LINUX_NS_PER_SEC);
    ts->tv_nsec = (int64_t)(ns % LINUX_NS_PER_SEC);

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(sysinfo) {
    (void)process; (void)thread;

    struct linux_sysinfo* info = (struct linux_sysinfo*)regs->arg1;

    if (!info || (uint64_t)(uintptr_t)info >= USER_VA_LIMIT ||
        (uint64_t)(uintptr_t)(info + 1) >= USER_VA_LIMIT)
        return SCRC(-EFAULT);

    memset(info, 0, sizeof(*info));
    info->uptime   = (int64_t)(arch_get_ns_time() / LINUX_NS_PER_SEC);
    info->mem_unit = 1;

    return SCRC(0);
}
