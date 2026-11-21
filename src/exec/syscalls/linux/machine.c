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
