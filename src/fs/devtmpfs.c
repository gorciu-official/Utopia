#include <fs/filesystem.h>
#include <mm/pmm.h>
#include <lib/screen.h>

typedef struct {
    vnode_t vnode;
} devtmpfs_node_t;

static vnode_ops_t devtmpfs_ops = {}; 

static vnode_t* devtmpfs_root_to_vnode(devtmpfs_node_t* root) {
    root->vnode.ops = &devtmpfs_ops;
    root->vnode.type = VNODE_TYPE_DIR;
    return (vnode_t*)root;
}

static int devtmpfs_mount(filesystem_mount_t* mount, const char* source, const char* target) {
    (void)source;
    (void)target;

    if (!mount) return -1;

    devtmpfs_node_t* root = malloc(sizeof(devtmpfs_node_t));
    if (!root) return -1;

    memset(root, 0, sizeof(devtmpfs_node_t));

    root->vnode.ops = &devtmpfs_ops;
    root->vnode.type = VNODE_TYPE_DIR;
    root->vnode.size = 0;
    root->vnode.mount = mount;
    root->vnode.fs_private = root;

    mount->fs_private = root;
    mount->root = devtmpfs_root_to_vnode(root);

    return 0;
}

filesystem_driver_t devtmpfs_driver = {
    .name = "devtmpfs",
    .mount = devtmpfs_mount
}; 

static int dev_read_eof(vnode_t* node, void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_read) {
    (void)node; (void)buffer; (void)size; (void)offset;
    *bytes_read = 0;
    return 0;
}

static int dev_write_discard(vnode_t* node, const void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_written) {
    (void)node; (void)buffer; (void)offset;
    *bytes_written = size;
    return 0;
}

static int dev_write_screen(vnode_t* node, const void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_written) {
    (void)node; (void)offset;
    user_print((char*)buffer, size);
    *bytes_written = size;
    return 0;
}

static const vnode_ops_t dev_console_ops = { .read = dev_read_eof, .write = dev_write_screen };
static const vnode_ops_t dev_null_ops    = { .read = dev_read_eof, .write = dev_write_discard };

static vnode_t dev_console_node = { .ops = &dev_console_ops, .type = VNODE_TYPE_FILE };
static vnode_t dev_kmsg_node    = { .ops = &dev_console_ops, .type = VNODE_TYPE_FILE };
static vnode_t dev_null_node    = { .ops = &dev_null_ops,    .type = VNODE_TYPE_FILE };

void devtmpfs_register_devices(void) {
    vfs_register_node("/dev/console", &dev_console_node);
    vfs_register_node("/dev/kmsg", &dev_kmsg_node);
    vfs_register_node("/dev/null", &dev_null_node);
}
