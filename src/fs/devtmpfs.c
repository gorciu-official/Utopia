#include <drivers/filesystem.h>
#include <memory.h>

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
