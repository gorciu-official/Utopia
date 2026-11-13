#include <fs/filesystem.h>
#include <mm/pmm.h>
#include <lib/string.h>

#define PROC_CMDLINE_MAX 1024

static char proc_cmdline_buffer[PROC_CMDLINE_MAX];

static int proc_cmdline_read(vnode_t* node, void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_read) {
    if (!node || !buffer || !bytes_read) return -1;

    if (offset >= node->size) {
        *bytes_read = 0;
        return 0;
    }

    if (offset + size > node->size) size = node->size - offset;
    memcpy(buffer, proc_cmdline_buffer + offset, size);
    *bytes_read = size;
    return 0;
}

static const vnode_ops_t proc_cmdline_ops = { .read = proc_cmdline_read };

static vnode_t proc_cmdline_node = { .ops = &proc_cmdline_ops, .type = VNODE_TYPE_FILE };

// there is no real procfs yet, so single nodes are pinned to their paths
void procfs_register_nodes(const char* cmdline) {
    int len = cmdline ? strlen(cmdline) : 0;
    if (len > PROC_CMDLINE_MAX - 2) len = PROC_CMDLINE_MAX - 2;

    if (len) memcpy(proc_cmdline_buffer, cmdline, len);
    proc_cmdline_buffer[len++] = '\n';

    proc_cmdline_node.size = len;
    vfs_register_node("/proc/cmdline", &proc_cmdline_node);
}

int procfs_parse_fd_path(const char* path, uint32_t pid) {
    const char* p;

    if (strncmp(path, "/proc/self/fd/", 14) == 0) {
        p = path + 14;
    } else if (strncmp(path, "/dev/fd/", 8) == 0) {
        p = path + 8;
    } else if (strncmp(path, "/proc/", 6) == 0) {
        p = path + 6;

        uint64_t number = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 10) return -1;
            number = number * 10 + (uint64_t)(*p - '0');
            p++;
        }

        if (digits == 0 || number != pid || strncmp(p, "/fd/", 4) != 0) return -1;
        p += 4;
    } else {
        return -1;
    }

    int fd = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 6) return -1;
        fd = fd * 10 + (*p - '0');
        p++;
    }

    // "/proc/self/fd/" alone, or anything after the number, is not a descriptor link
    if (digits == 0 || *p) return -1;
    return fd;
}
