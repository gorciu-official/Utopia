#include <types.h>
#include <lib/string.h>
#include <drivers/filesystem.h>
#include <memory.h>
#include <lib/screen.h>

#define container_of(ptr, type, member) \
    ((type *)((char*)(ptr) - offsetof(type, member)))

struct ramfs_node;

typedef struct ramfs_node {
    vnode_t vnode;
    char name[64];

    struct ramfs_node* parent;
    struct ramfs_node* children;
    struct ramfs_node* next;

    uint8_t* data;
} ramfs_node_t;

static vnode_ops_t ramfs_ops;

static ramfs_node_t* ramfs_create_node(const char* name, vnode_type_t type) {
    if (!name) return 0;

    ramfs_node_t* node = malloc(sizeof(ramfs_node_t));
    if (!node) return 0;

    memset(node, 0, sizeof(ramfs_node_t));

    node->vnode.ops = &ramfs_ops;
    node->vnode.type = type;
    node->vnode.size = 0;

    int i = 0;
    while (name[i] && i < 63) {
        node->name[i] = name[i];
        i++;
    }
    node->name[i] = 0;

    return node;
}

static ramfs_node_t* ramfs_find_child(ramfs_node_t* parent, const char* name) {
    if (!parent || !name) return 0;
    ramfs_node_t* cur = parent->children;

    while (cur) {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }

    return 0;
}

static void ramfs_add_child(ramfs_node_t* parent, ramfs_node_t* child) {
    if (!parent || !child) return;
    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

static vnode_t* ramfs_to_vnode(ramfs_node_t* node) {
    return (vnode_t*)node;
}

static ramfs_node_t* vnode_to_ramfs(vnode_t* v) {
    return (ramfs_node_t*)v;
}

static int ramfs_lookup(vnode_t* parent, const char* name, vnode_t** result) {
    if (!parent || !name || !result) return -1;

    ramfs_node_t* p = vnode_to_ramfs(parent);
    if (p->vnode.type != VNODE_TYPE_DIR) return -1;

    ramfs_node_t* found = ramfs_find_child(p, name);
    if (!found) return -1;

    *result = ramfs_to_vnode(found);
    return 0;
}

static int ramfs_mkdir(vnode_t* parent, const char* name, vnode_t** result) {
    if (!parent || !name || !result) return -1;

    ramfs_node_t* p = vnode_to_ramfs(parent);
    if (p->vnode.type != VNODE_TYPE_DIR) return -1;

    if (ramfs_find_child(p, name))
        return -1;

    ramfs_node_t* dir = ramfs_create_node(name, VNODE_TYPE_DIR);
    if (!dir) return -1;

    ramfs_add_child(p, dir);
    *result = ramfs_to_vnode(dir);

    return 0;
}

static int ramfs_create(vnode_t* parent, const char* name, vnode_t** result) {
    if (!parent || !name || !result) return -1;

    ramfs_node_t* p = vnode_to_ramfs(parent);
    if (p->vnode.type != VNODE_TYPE_DIR) return -1;

    if (ramfs_find_child(p, name))
        return -1;

    ramfs_node_t* file = ramfs_create_node(name, VNODE_TYPE_FILE);
    if (!file) return -1;

    ramfs_add_child(p, file);
    *result = ramfs_to_vnode(file);

    return 0;
}

static int ramfs_read(vnode_t* node, void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_read) {
    if (!node || !buffer || !bytes_read) return -1;

    ramfs_node_t* n = vnode_to_ramfs(node);
    if (n->vnode.type != VNODE_TYPE_FILE)
        return -1;

    if (!n->data || offset >= n->vnode.size) {
        *bytes_read = 0;
        return 0;
    }

    uint64_t to_read = size;
    if (offset + to_read > n->vnode.size)
        to_read = n->vnode.size - offset;

    memcpy(buffer, n->data + offset, to_read);

    *bytes_read = to_read;
    return 0;
}

static int ramfs_write(vnode_t* node, const void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_written) {
    if (!node || !buffer || !bytes_written) return -1;

    ramfs_node_t* n = vnode_to_ramfs(node);
    if (n->vnode.type != VNODE_TYPE_FILE)
        return -1;

    uint64_t needed = offset + size;

    if (needed > n->vnode.size) {
        uint8_t* new_data = malloc(needed);
        if (!new_data) return -1;

        memset(new_data, 0, needed);

        if (n->data) {
            memcpy(new_data, n->data, n->vnode.size);
            free(n->data);
        }

        n->data = new_data;
        n->vnode.size = needed;
    }

    memcpy(n->data + offset, buffer, size);

    *bytes_written = size;

    return 0;
}

static vnode_ops_t ramfs_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir  = ramfs_mkdir,
    .read   = ramfs_read,
    .write  = ramfs_write
};

static vnode_t* ramfs_root_to_vnode(ramfs_node_t* root) {
    root->vnode.ops = &ramfs_ops;
    root->vnode.type = VNODE_TYPE_DIR;
    return (vnode_t*)root;
}

static int ramfs_mount(filesystem_mount_t* mount, const char* source, const char* target) {
    (void)source;
    (void)target;

    if (!mount) return -1;

    ramfs_node_t* root = malloc(sizeof(ramfs_node_t));
    if (!root) return -1;

    memset(root, 0, sizeof(ramfs_node_t));

    root->vnode.ops = &ramfs_ops;
    root->vnode.type = VNODE_TYPE_DIR;
    root->vnode.size = 0;
    root->vnode.mount = mount;
    root->vnode.fs_private = root;

    mount->fs_private = root;
    mount->root = ramfs_root_to_vnode(root);

    return 0;
}

static int ramfs_unmount(filesystem_mount_t* mount) {
    // i am lazy
    (void)mount;
    return 0;
}

filesystem_driver_t ramfs_driver = {
    .name = "ramfs",
    .mount = ramfs_mount,
    .unmount = ramfs_unmount
};
