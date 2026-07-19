#pragma once

#include <types.h>

typedef enum {
    VNODE_TYPE_FILE = 0,
    VNODE_TYPE_DIR = 1
} vnode_type_t;

struct vnode;

typedef struct vnode_ops {
    int (*lookup)(struct vnode *parent, const char *name, struct vnode **result);
    int (*create)(struct vnode *parent, const char *name, struct vnode **result);
    int (*mkdir)(struct vnode *parent, const char *name, struct vnode **result);
    int (*read)(struct vnode *node, void *buffer, uint64_t size, uint64_t offset, uint64_t *bytes_read);
    int (*write)(struct vnode *node, const void *buffer, uint64_t size, uint64_t offset, uint64_t *bytes_written);
} vnode_ops_t;

typedef struct vnode {
    const vnode_ops_t *ops;

    vnode_type_t type;
    uint64_t size;

    struct filesystem_mount *mount;
    void *fs_private;
} vnode_t;

typedef struct filesystem_driver {
    const char *name;

    int (*mount)(struct filesystem_mount *mount, const char *source, const char *target);
    int (*unmount)(struct filesystem_mount *mount);
} filesystem_driver_t;

typedef struct filesystem_mount {
    vnode_t *root;
    void *fs_private;
    filesystem_driver_t *fs;
} filesystem_mount_t;

void vfs_init(void);
int vfs_register_driver(filesystem_driver_t *driver);
int vfs_mount(const char *type, const char *source, const char *target);
int vfs_lookup(const char *path, vnode_t **result);
extern filesystem_driver_t ramfs_driver;
extern filesystem_driver_t tarfs_driver;

void tarfs_set_image(const void* data, uint64_t size);
