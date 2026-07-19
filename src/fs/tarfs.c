#include <types.h>
#include <lib/string.h>
#include <drivers/filesystem.h>
#include <memory.h>
#include <lib/screen.h>

#define container_of(ptr, type, member) \
    ((type *)((char*)(ptr) - offsetof(type, member)))

struct tarfs_node;

typedef struct tarfs_node {
    vnode_t vnode;
    char name[64];

    struct tarfs_node* parent;
    struct tarfs_node* children;
    struct tarfs_node* next;

    const uint8_t* data; 
} tarfs_node_t;

typedef struct __attribute__((packed)) {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_header_t;

static vnode_ops_t tarfs_ops;

static const uint8_t* tarfs_image = 0;
static uint64_t tarfs_image_size = 0;

void tarfs_set_image(const void* data, uint64_t size) {
    // TODO: this is vulnerable to timing, since we have SMP 
    //       another thread may want to mount tarfs and break 
    //       things
    tarfs_image = (const uint8_t*)data;
    tarfs_image_size = size;
}

static uint64_t tarfs_oct2u64(const char* str, int len) {
    uint64_t val = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '7') break;
        val = (val << 3) + (uint64_t)(c - '0');
    }
    return val;
}

static uint64_t tarfs_align_up(uint64_t n) {
    return (n + 511ULL) & ~511ULL;
}

static int tarfs_block_is_zero(const uint8_t* block) {
    for (int i = 0; i < 512; i++) {
        if (block[i] != 0) return 0;
    }
    return 1;
}

static tarfs_node_t* tarfs_create_node(const char* name, vnode_type_t type) {
    if (!name) return 0;

    tarfs_node_t* node = malloc(sizeof(tarfs_node_t));
    if (!node) return 0;

    memset(node, 0, sizeof(tarfs_node_t));

    node->vnode.ops = &tarfs_ops;
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

static tarfs_node_t* tarfs_find_child(tarfs_node_t* parent, const char* name, int len) {
    if (!parent || !name) return 0;
    tarfs_node_t* cur = parent->children;

    while (cur) {
        if ((int)strlen(cur->name) == len && memcmp(cur->name, name, len) == 0)
            return cur;
        cur = cur->next;
    }

    return 0;
}

static void tarfs_add_child(tarfs_node_t* parent, tarfs_node_t* child) {
    if (!parent || !child) return;
    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

static vnode_t* tarfs_to_vnode(tarfs_node_t* node) {
    return (vnode_t*)node;
}

static tarfs_node_t* vnode_to_tarfs(vnode_t* v) {
    return (tarfs_node_t*)v;
}

static tarfs_node_t* tarfs_resolve_dir_path(tarfs_node_t* root, const char* path, int path_len) {
    tarfs_node_t* cur = root;
    int i = 0;

    while (i < path_len) {
        int start = i;
        while (i < path_len && path[i] != '/') i++;
        int comp_len = i - start;

        if (comp_len > 0) {
            tarfs_node_t* child = tarfs_find_child(cur, path + start, comp_len);
            if (!child) {
                char name[64];
                int n = comp_len < 63 ? comp_len : 63;
                memcpy(name, path + start, n);
                name[n] = 0;

                child = tarfs_create_node(name, VNODE_TYPE_DIR);
                if (!child) return 0;
                tarfs_add_child(cur, child);
            }
            cur = child;
        }

        i++;
    }

    return cur;
}

static int tarfs_lookup(vnode_t* parent, const char* name, vnode_t** result) {
    if (!parent || !name || !result) return -1;

    tarfs_node_t* p = vnode_to_tarfs(parent);
    if (p->vnode.type != VNODE_TYPE_DIR) return -1;

    tarfs_node_t* found = tarfs_find_child(p, name, (int)strlen(name));
    if (!found) return -1;

    *result = tarfs_to_vnode(found);
    return 0;
}

static int tarfs_create(vnode_t* parent, const char* name, vnode_t** result) {
    (void)parent; (void)name; (void)result;
    return -1; /* read-only */
}

static int tarfs_mkdir(vnode_t* parent, const char* name, vnode_t** result) {
    (void)parent; (void)name; (void)result;
    return -1; /* read-only */
}

static int tarfs_read(vnode_t* node, void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_read) {
    if (!node || !buffer || !bytes_read) return -1;

    tarfs_node_t* n = vnode_to_tarfs(node);
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

static int tarfs_write(vnode_t* node, const void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_written) {
    (void)node; (void)buffer; (void)size; (void)offset; (void)bytes_written;
    return -1; /* read-only */
}

static vnode_ops_t tarfs_ops = {
    .lookup = tarfs_lookup,
    .create = tarfs_create,
    .mkdir  = tarfs_mkdir,
    .read   = tarfs_read,
    .write  = tarfs_write
};

static int tarfs_build_path(const tar_header_t* hdr, char* out, int out_cap) {
    int len = 0;

    int prefix_len = 0;
    while (prefix_len < 155 && hdr->prefix[prefix_len]) prefix_len++;

    int name_len = 0;
    while (name_len < 100 && hdr->name[name_len]) name_len++;

    if (prefix_len > 0) {
        int n = prefix_len < out_cap ? prefix_len : out_cap;
        memcpy(out, hdr->prefix, n);
        len = n;
        if (len < out_cap) out[len++] = '/';
    }

    int room = out_cap - len;
    int n = name_len < room ? name_len : room;
    if (n > 0) memcpy(out + len, hdr->name, n);
    len += n;

    while (len > 0 && out[len - 1] == '/') len--;

    int start = 0;
    if (len >= 2 && out[0] == '.' && out[1] == '/') start = 2;
    else if (len >= 1 && out[0] == '/') start = 1;

    if (start > 0) {
        memmove(out, out + start, len - start);
        len -= start;
    }

    return len;
}

static void tarfs_parse_archive(tarfs_node_t* root) {
    if (!tarfs_image || tarfs_image_size < 512) return;

    uint64_t offset = 0;

    while (offset + 512 <= tarfs_image_size) {
        const tar_header_t* hdr = (const tar_header_t*)(tarfs_image + offset);

        if (tarfs_block_is_zero((const uint8_t*)hdr)) break;

        uint64_t size = tarfs_oct2u64(hdr->size, 12);
        char typeflag = hdr->typeflag;

        char path[256];
        int path_len = tarfs_build_path(hdr, path, (int)sizeof(path));

        offset += 512;

        if (path_len > 0 && (typeflag == '0' || typeflag == 0 || typeflag == '5')) {
            int split = path_len;
            while (split > 0 && path[split - 1] != '/') split--;

            const char* base = path + split;
            int base_len = path_len - split;

            tarfs_node_t* parent = tarfs_resolve_dir_path(root, path, split > 0 ? split - 1 : 0);
            if (parent && base_len > 0) {
                tarfs_node_t* existing = tarfs_find_child(parent, base, base_len);

                if (typeflag == '5') {
                    if (!existing) {
                        char name[64];
                        int n = base_len < 63 ? base_len : 63;
                        memcpy(name, base, n);
                        name[n] = 0;
                        tarfs_node_t* dir = tarfs_create_node(name, VNODE_TYPE_DIR);
                        if (dir) tarfs_add_child(parent, dir);
                    }
                } else if (!existing) {
                    char name[64];
                    int n = base_len < 63 ? base_len : 63;
                    memcpy(name, base, n);
                    name[n] = 0;

                    tarfs_node_t* file = tarfs_create_node(name, VNODE_TYPE_FILE);
                    if (file) {
                        file->data = tarfs_image + offset;
                        file->vnode.size = size;
                        tarfs_add_child(parent, file);
                    }
                }
            }
        }

        offset += tarfs_align_up(size);
    }
}

static int tarfs_mount(filesystem_mount_t* mount, const char* source, const char* target) {
    (void)source;
    (void)target;

    if (!mount) return -1;

    tarfs_node_t* root = malloc(sizeof(tarfs_node_t));
    if (!root) return -1;

    memset(root, 0, sizeof(tarfs_node_t));

    root->vnode.ops = &tarfs_ops;
    root->vnode.type = VNODE_TYPE_DIR;
    root->vnode.size = 0;
    root->vnode.mount = mount;
    root->vnode.fs_private = root;

    mount->fs_private = root;
    mount->root = (vnode_t*)root;

    tarfs_parse_archive(root);

    return 0;
}

static int tarfs_unmount(filesystem_mount_t* mount) {
    /* nodes are never freed; same laziness as ramfs lol */
    (void)mount;
    return 0;
}

filesystem_driver_t tarfs_driver = {
    .name = "tarfs",
    .mount = tarfs_mount,
    .unmount = tarfs_unmount
};
