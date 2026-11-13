#pragma once

#include <types.h>

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_DIRECTORY 0x10000

#define VFS_PATH_MAX     256

#define VFS_AT_FDCWD     (-100)

#define VFS_EIO           5
#define VFS_ENOMEM        12
#define VFS_EBUSY         16
#define VFS_ENODEV        19
#define VFS_ENOTDIR       20
#define VFS_ENOENT        2
#define VFS_EBADF         9
#define VFS_EINVAL        22
#define VFS_ERANGE        34
#define VFS_ENAMETOOLONG  36
#define VFS_ENOSYS        38

#define VFS_MS_RDONLY      0x00000001UL
#define VFS_MS_NOSUID      0x00000002UL
#define VFS_MS_NODEV       0x00000004UL
#define VFS_MS_NOEXEC      0x00000008UL
#define VFS_MS_REMOUNT     0x00000020UL
#define VFS_MS_BIND        0x00001000UL
#define VFS_MS_MOVE        0x00002000UL
#define VFS_MS_REC         0x00004000UL
#define VFS_MS_UNBINDABLE  0x00020000UL
#define VFS_MS_PRIVATE     0x00040000UL
#define VFS_MS_SLAVE       0x00080000UL
#define VFS_MS_SHARED      0x00100000UL

#define VFS_MNT_FORCE      0x1
#define VFS_MNT_DETACH     0x2

typedef enum {
    VNODE_TYPE_FILE = 0,
    VNODE_TYPE_DIR = 1
} vnode_type_t;

struct vnode;

typedef struct vfs_dirent {
    uint64_t ino;
    uint64_t off;
    uint8_t type;
    const char *name;
} vfs_dirent_t;

typedef struct vnode_ops {
    int (*lookup)(struct vnode *parent, const char *name, struct vnode **result);
    int (*create)(struct vnode *parent, const char *name, struct vnode **result);
    int (*mkdir)(struct vnode *parent, const char *name, struct vnode **result);
    int (*read)(struct vnode *node, void *buffer, uint64_t size, uint64_t offset, uint64_t *bytes_read);
    int (*write)(struct vnode *node, const void *buffer, uint64_t size, uint64_t offset, uint64_t *bytes_written);
    int (*readdir)(struct vnode *node, uint64_t index, vfs_dirent_t *entry);
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

    char *target;          
    unsigned long flags;
} filesystem_mount_t;

void vfs_init(void);
int vfs_register_driver(filesystem_driver_t *driver);
int vfs_mount(const char *type, const char *source, const char *target, unsigned long flags);
int vfs_remount(const char *target, unsigned long flags);
int vfs_bind_mount(const char *source, const char *target);
int vfs_umount(const char *target, int flags);
int vfs_lookup(const char *path, vnode_t **result);
int vfs_open(const char *path, int flags, vnode_t **result);
int vfs_mkdir(const char *path);
int vfs_register_node(const char *path, vnode_t *node);

int vfs_normalize_path(const char *path, char *out);

int vfs_resolve_path(const char *base, const char *path, char *out);

int procfs_parse_fd_path(const char *path, uint32_t pid);

void devtmpfs_register_devices(void);
void procfs_register_nodes(const char *cmdline);

// -- fs drivers 
extern filesystem_driver_t ramfs_driver;
extern filesystem_driver_t tarfs_driver;
extern filesystem_driver_t devtmpfs_driver;

void tarfs_set_image(const void* data, uint64_t size);
