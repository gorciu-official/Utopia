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
