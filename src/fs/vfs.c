#include <types.h>
#include <lib/string.h>
#include <fs/filesystem.h>
#include <mm/pmm.h>
#include <lib/screen.h>

#define MAX_DRIVERS 16
#define MAX_MOUNTS 16
#define PATH_MAX 256

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

void vfs_init() {
    // it quite literally does nothing but maybe it will need to do 
    // something in the future?
    printk(KMT_LOG, "Filesystem", "VFS initialized successfully.");
}

int vfs_mount(const char* type, const char* source, const char* target) {
    if (!type) {
        return -1;
    }

    filesystem_driver_t* driver = vfs_find_driver(type);

    if (!driver) {
        printk(KMT_LOG, "Filesystem", "Unknown filesystem: %s", type);
        return -1;
    }

    if (g_mount_count >= MAX_MOUNTS) {
        return -1;
    }

    filesystem_mount_t* mount = malloc(sizeof(filesystem_mount_t));

    if (!mount) {
        return -1;
    }

    mount->fs = driver;
    mount->fs_private = 0;
    mount->root = 0;

    if (!driver->mount) {
        free(mount);
        return -1;
    }

    if (driver->mount(mount, source, target) != 0) {
        printk(KMT_LOG, "Filesystem", "Mount failed for %s", type);
        free(mount);
        return -1;
    }

    g_mounts[g_mount_count++] = mount;

    printk(KMT_LOG, "Filesystem", "Mounted filesystem %s at %s", type, target);
    return 0;
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
        if (i < 127) {
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

    for (uint32_t i = 0; i < g_static_node_count; i++) {
        if (strcmp(g_static_nodes[i].path, path) == 0) {
            *result = g_static_nodes[i].node;
            return 0;
        }
    }

    if (path[0] != '/') {
        if (path[0] == '.' && (path[1] == '\0' || path[1] == '/')) {
            path++;

            if (*path == '/') {
                path++;
            }
        }
    }

    filesystem_mount_t* mount = g_mounts[0];
    if (!mount || !mount->root) {
        return -1;
    }

    vnode_t* current = mount->root;

    char token[128];
    const char* p = path;

    while (p && *p) {
        p = vfs_next_token(p, token);

        if (token[0] == 0) 
            continue;

        if (token[0] == '.' && token[1] == '\0') {
            current = mount->root;
            continue;
        }

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
