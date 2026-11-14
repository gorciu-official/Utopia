#include <types.h>
#include <lib/string.h>
#include <fs/filesystem.h>
#include <mm/pmm.h>
#include <lib/screen.h>

#define MAX_DRIVERS 16
#define MAX_MOUNTS 64
#define PATH_MAX VFS_PATH_MAX
#define NAME_MAX 127

#define MOUNT_STORED_FLAGS (VFS_MS_RDONLY | VFS_MS_NOSUID | VFS_MS_NODEV | VFS_MS_NOEXEC)

static filesystem_driver_t* g_drivers[MAX_DRIVERS];
static uint32_t g_driver_count = 0;

static filesystem_mount_t* g_mounts[MAX_MOUNTS];
static uint32_t g_mount_count = 0;

int vfs_register_driver(filesystem_driver_t* driver) {
    if (!driver || !driver->name) {
        return -1;
    }

    if (g_driver_count >= MAX_DRIVERS) {
        return -1;
    }

    g_drivers[g_driver_count++] = driver;

    printk(KMT_LOG, "Filesystem", "Registered filesystem driver: %s", driver->name);
    return 0;
}

static filesystem_driver_t* vfs_find_driver(const char* type) {
    if (!type) {
        return 0;
    }

    uint32_t i = 0;

    while (i < g_driver_count) {
        if (strcmp(g_drivers[i]->name, type) == 0) {
            return g_drivers[i];
        }
        i++;
    }

    return 0;
}

static const struct {
    const char* type;
    const char* backing;
} g_fs_aliases[] = {
    { "tmpfs",      "ramfs" },
    { "proc",       "ramfs" },
    { "sysfs",      "ramfs" },
    { "devpts",     "ramfs" },
    { "cgroup",     "ramfs" },
    { "cgroup2",    "ramfs" },
};

#define FS_ALIAS_COUNT (sizeof(g_fs_aliases) / sizeof(g_fs_aliases[0]))

static filesystem_driver_t* vfs_resolve_driver(const char* type) {
    filesystem_driver_t* driver = vfs_find_driver(type);
    if (driver) {
        return driver;
    }

    for (uint32_t i = 0; i < FS_ALIAS_COUNT; i++) {
        if (strcmp(g_fs_aliases[i].type, type) != 0) {
            continue;
        }

        driver = vfs_find_driver(g_fs_aliases[i].backing);
        return driver;
    }

    return 0;
}

void vfs_init() {
    // it quite literally does nothing but maybe it will need to do 
    // something in the future?
    printk(KMT_LOG, "Filesystem", "VFS initialized successfully.");
}

static int vfs_normalize_append(const char* path, char* out, int len) {
    const char* p = path;

    while (*p) {
        while (*p == '/') {
            p++;
        }
        if (!*p) {
            break;
        }

        const char* start = p;
        while (*p && *p != '/') {
            p++;
        }
        int clen = (int)(p - start);

        if (clen == 1 && start[0] == '.') {
            continue;
        }

        if (clen == 2 && start[0] == '.' && start[1] == '.') {
            while (len > 1 && out[len - 1] != '/') {
                len--;
            }
            if (len > 1) {
                len--;
            }
            out[len] = '\0';
            continue;
        }

        if (clen > NAME_MAX) {
            return -VFS_ENAMETOOLONG;
        }
        if (len + (len > 1 ? 1 : 0) + clen >= PATH_MAX) {
            return -VFS_ENAMETOOLONG;
        }

        if (len > 1) {
            out[len++] = '/';
        }
        memcpy(out + len, start, clen);
        len += clen;
        out[len] = '\0';
    }

    return 0;
}

int vfs_normalize_path(const char* path, char* out) {
    if (!path || !path[0]) {
        return -VFS_ENOENT;
    }

    out[0] = '/';
    out[1] = '\0';

    return vfs_normalize_append(path, out, 1);
}

int vfs_resolve_path(const char* base, const char* path, char* out) {
    if (!path || !path[0]) {
        return -VFS_ENOENT;
    }

    if (path[0] == '/' || !base || base[0] != '/') {
        return vfs_normalize_path(path, out);
    }

    int blen = strlen(base);
    if (blen >= PATH_MAX) {
        return -VFS_ENAMETOOLONG;
    }

    memcpy(out, base, blen + 1);

    return vfs_normalize_append(path, out, blen);
}

static bool vfs_path_has_prefix(const char* path, const char* prefix, int plen) {
    if (plen == 1) {
        return true; 
    }

    return strncmp(path, prefix, plen) == 0 && (path[plen] == '\0' || path[plen] == '/');
}

static filesystem_mount_t* vfs_find_mount(const char* norm, const char** rest) {
    filesystem_mount_t* best = 0;
    int best_len = 0;

    for (int i = (int)g_mount_count - 1; i >= 0; i--) {
        filesystem_mount_t* m = g_mounts[i];
        int tlen = strlen(m->target);

        if (best && tlen <= best_len) {
            continue;
        }
        if (!vfs_path_has_prefix(norm, m->target, tlen)) {
            continue;
        }

        best = m;
        best_len = tlen;
    }

    if (best && rest) {
        *rest = norm + (best_len > 1 ? best_len : 0);
    }

    return best;
}

static int vfs_find_exact_mount(const char* norm) {
    for (int i = (int)g_mount_count - 1; i >= 0; i--) {
        if (strcmp(g_mounts[i]->target, norm) == 0) {
            return i;
        }
    }

    return -1;
}

static filesystem_mount_t* vfs_new_mount(const char* norm, unsigned long flags) {
    filesystem_mount_t* mount = malloc(sizeof(filesystem_mount_t));
    if (!mount) {
        return 0;
    }

    memset(mount, 0, sizeof(filesystem_mount_t));

    int len = strlen(norm);
    mount->target = malloc(len + 1);
    if (!mount->target) {
        free(mount);
        return 0;
    }

    memcpy(mount->target, norm, len + 1);
    mount->flags = flags;
    return mount;
}

static void vfs_free_mount(filesystem_mount_t* mount) {
    free(mount->target);
    free(mount);
}

static const char* vfs_next_token(const char* path, char* out) {
    if (!path || !out) {
        return 0;
    }

    while (*path == '/') {
        path++;
    }

    int i = 0;

    while (*path && *path != '/') {
        if (i < NAME_MAX) {
            out[i++] = *path;
        }
        path++;
    }

    out[i] = 0;
    return path;
}

#define MAX_STATIC_NODES 16

static struct {
    const char* path;
    vnode_t*    node;
} g_static_nodes[MAX_STATIC_NODES];
static uint32_t g_static_node_count = 0;

int vfs_register_node(const char* path, vnode_t* node) {
    if (!path || !node || path[0] != '/' || g_static_node_count >= MAX_STATIC_NODES) {
        return -1;
    }

    g_static_nodes[g_static_node_count].path = path;
    g_static_nodes[g_static_node_count].node = node;
    g_static_node_count++;
    return 0;
}

int vfs_lookup(const char* path, vnode_t** result) {
    if (!path || !result) {
        return -1;
    }

    char norm[PATH_MAX];
    if (vfs_normalize_path(path, norm) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_static_node_count; i++) {
        if (strcmp(g_static_nodes[i].path, norm) == 0) {
            *result = g_static_nodes[i].node;
            return 0;
        }
    }

    const char* rest = 0;
    filesystem_mount_t* mount = vfs_find_mount(norm, &rest);
    if (!mount || !mount->root) {
        return -1;
    }

    vnode_t* current = mount->root;

    char token[NAME_MAX + 1];
    const char* p = rest;

    while (p && *p) {
        p = vfs_next_token(p, token);

        if (token[0] == 0) 
            continue;

        if (!current || !current->ops || !current->ops->lookup) 
            return -1;

        vnode_t* next = 0;

        if (current->ops->lookup(current, token, &next) != 0)
            return -1;

        if (!next) 
            return -1;

        current = next;
    }

    *result = current;
    return 0;
}

static int vfs_check_mountpoint(const char* norm) {
    if (g_mount_count == 0) {
        return strcmp(norm, "/") == 0 ? 0 : -VFS_ENOENT;
    }

    vnode_t* node = 0;
    if (vfs_lookup(norm, &node) != 0 || !node) {
        return -VFS_ENOENT;
    }
    if (node->type != VNODE_TYPE_DIR) {
        return -VFS_ENOTDIR;
    }

    return 0;
}

int vfs_mount(const char* type, const char* source, const char* target, unsigned long flags) {
    if (!type || !target) {
        return -VFS_EINVAL;
    }

    char norm[PATH_MAX];
    int err = vfs_normalize_path(target, norm);
    if (err) {
        return err;
    }

    filesystem_driver_t* driver = vfs_resolve_driver(type);

    if (!driver || !driver->mount) {
        printk(KMT_LOG, "Filesystem", "Unknown filesystem: %s", type);
        return -VFS_ENODEV;
    }

    if (g_mount_count >= MAX_MOUNTS) {
        return -VFS_ENOMEM;
    }

    err = vfs_check_mountpoint(norm);
    if (err) {
        return err;
    }

    filesystem_mount_t* mount = vfs_new_mount(norm, flags & MOUNT_STORED_FLAGS);
    if (!mount) {
        return -VFS_ENOMEM;
    }

    mount->fs = driver;

    int res = driver->mount(mount, source, norm);
    if (res != 0 || !mount->root) {
        printk(KMT_LOG, "Filesystem", "Mount failed for %s", type);
        vfs_free_mount(mount);
        return res < -1 ? res : -VFS_EIO;
    }

    g_mounts[g_mount_count++] = mount;

    printk(KMT_LOG, "Filesystem", "Mounted filesystem %s at %s", type, norm);

    return 0;
}

int vfs_remount(const char* target, unsigned long flags) {
    char norm[PATH_MAX];
    int err = vfs_normalize_path(target, norm);
    if (err) {
        return err;
    }

    int idx = vfs_find_exact_mount(norm);
    if (idx < 0) {
        return -VFS_EINVAL; 
    }

    g_mounts[idx]->flags = (g_mounts[idx]->flags & ~MOUNT_STORED_FLAGS) | (flags & MOUNT_STORED_FLAGS);
    return 0;
}

int vfs_bind_mount(const char* source, const char* target) {
    char snorm[PATH_MAX];
    char tnorm[PATH_MAX];

    int err = vfs_normalize_path(source, snorm);
    if (err) {
        return err;
    }
    err = vfs_normalize_path(target, tnorm);
    if (err) {
        return err;
    }

    vnode_t* src = 0;
    if (vfs_lookup(snorm, &src) != 0 || !src) {
        return -VFS_ENOENT;
    }
    if (src->type != VNODE_TYPE_DIR) {
        return -VFS_ENOTDIR;
    }

    if (g_mount_count >= MAX_MOUNTS) {
        return -VFS_ENOMEM;
    }

    err = vfs_check_mountpoint(tnorm);
    if (err) {
        return err;
    }

    filesystem_mount_t* src_mount = vfs_find_mount(snorm, 0);
    if (!src_mount) {
        return -VFS_EINVAL;
    }

    filesystem_mount_t* mount = vfs_new_mount(tnorm, src_mount->flags | VFS_MS_BIND);
    if (!mount) {
        return -VFS_ENOMEM;
    }

    mount->fs = src_mount->fs;
    mount->fs_private = src_mount->fs_private;
    mount->root = src;

    g_mounts[g_mount_count++] = mount;

    printk(KMT_LOG, "Filesystem", "Bound %s at %s", snorm, tnorm);
    return 0;
}

static void vfs_drop_mount(int idx) {
    filesystem_mount_t* mount = g_mounts[idx];

    if (!(mount->flags & VFS_MS_BIND) && mount->fs && mount->fs->unmount) {
        mount->fs->unmount(mount);
    }

    for (int i = idx; i + 1 < (int)g_mount_count; i++) {
        g_mounts[i] = g_mounts[i + 1];
    }
    g_mounts[--g_mount_count] = 0;

    printk(KMT_LOG, "Filesystem", "Unmounted %s", mount->target);
    vfs_free_mount(mount);
}

int vfs_umount(const char* target, int flags) {
    char norm[PATH_MAX];
    int err = vfs_normalize_path(target, norm);
    if (err) {
        return err;
    }

    if (strcmp(norm, "/") == 0) {
        return -VFS_EBUSY; 
    }

    int idx = vfs_find_exact_mount(norm);
    if (idx < 0) {
        return -VFS_EINVAL; 
    }

    int tlen = strlen(norm);

    for (int i = (int)g_mount_count - 1; i >= 0; i--) {
        filesystem_mount_t* m = g_mounts[i];

        if (strlen(m->target) <= tlen || !vfs_path_has_prefix(m->target, norm, tlen)) {
            continue;
        }

        if (!(flags & VFS_MNT_DETACH)) {
            return -VFS_EBUSY;
        }

        vfs_drop_mount(i);
    }

    idx = vfs_find_exact_mount(norm);
    if (idx >= 0) {
        vfs_drop_mount(idx);
    }

    return 0;
}

static int vfs_split_path(const char* path, char* parent_out, char* filename_out) {
    int len = 0;
    while (path[len]) len++;
    if (len == 0 || path[0] != '/') return -1;
    
    int last_slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash == -1) return -1;
    
    if (last_slash == 0) {
        strcpy(parent_out, "/");
        strcpy(filename_out, path + 1);
    } else {
        memcpy(parent_out, path, last_slash);
        parent_out[last_slash] = '\0';
        strcpy(filename_out, path + last_slash + 1);
    }
    return 0;
}

int vfs_open(const char* path, int flags, vnode_t** result) {
    if (!path || !result) {
        return -22; // -EINVAL
    }

    vnode_t* node = NULL;
    int lookup_res = vfs_lookup(path, &node);
    
    if (lookup_res == 0) {
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
            return -17; // -EEXIST
        }
        if ((flags & O_DIRECTORY) && node->type != VNODE_TYPE_DIR) {
            return -20; // -ENOTDIR
        }
        *result = node;
        return 0;
    }
    
    if (flags & O_CREAT) {
        char parent_path[PATH_MAX];
        char filename[128];
        if (vfs_split_path(path, parent_path, filename) != 0) {
            return -2; // -ENOENT
        }
        
        vnode_t* parent_node = NULL;
        if (vfs_lookup(parent_path, &parent_node) != 0) {
            return -2; // -ENOENT
        }
        
        if (parent_node->type != VNODE_TYPE_DIR) {
            return -20; // -ENOTDIR
        }

        if (!parent_node->ops || !parent_node->ops->create) {
            return -30; // -EROFS
        }
        
        vnode_t* new_node = NULL;
        if (parent_node->ops->create(parent_node, filename, &new_node) != 0) {
            return -5; // -EIO
        }
        
        *result = new_node;
        return 0;
    }
    
    return -2; // -ENOENT
}


int vfs_mkdir(const char* path) {
    if (!path || !path[0]) {
        return -2; // -ENOENT
    }

    char full[PATH_MAX];
    int off = 0;
    int len = strlen(path);

    if (path[0] != '/') {
        full[off++] = '/';
    }
    if (len + off >= PATH_MAX) {
        return -36; // -ENAMETOOLONG
    }
    memcpy(full + off, path, len + 1);
    len += off;

    while (len > 1 && full[len - 1] == '/') {
        full[--len] = '\0';
    }

    vnode_t* existing = NULL;
    if (vfs_lookup(full, &existing) == 0) {
        return -17; // -EEXIST
    }

    char parent_path[PATH_MAX];
    char name[128];
    if (vfs_split_path(full, parent_path, name) != 0 || name[0] == '\0') {
        return -2; // -ENOENT
    }

    vnode_t* parent = NULL;
    if (vfs_lookup(parent_path, &parent) != 0) {
        return -2; // -ENOENT
    }
    if (parent->type != VNODE_TYPE_DIR) {
        return -20; // -ENOTDIR
    }
    if (!parent->ops || !parent->ops->mkdir) {
        return -30; // -EROFS
    }

    vnode_t* created = NULL;
    int res = parent->ops->mkdir(parent, name, &created);
    if (res == 0) {
        return 0;
    }

    return res < -1 ? res : -5; // -EIO
}
