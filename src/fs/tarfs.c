#include <types.h>
#include <lib/string.h>
#include <drivers/filesystem.h>
#include <memory.h>
#include <lib/screen.h>

#define TARFS_MAX_SYMLINK_DEPTH 8

typedef struct tarfs_node {
    vnode_t vnode;
    char name[256];
    struct tarfs_node *parent, *children, *next;
    const uint8_t* data;
    char link_target[256];
    uint64_t ino; 
    uint8_t is_symlink; 
} tarfs_node_t;

typedef struct __attribute__((packed)) {
    char name[100], mode[8], uid[8], gid[8], size[12], mtime[12], chksum[8];
    char typeflag;
    char linkname[100], magic[6], version[2], uname[32], gname[32];
    char devmajor[8], devminor[8], prefix[155], pad[12];
} tar_header_t;

static vnode_ops_t tarfs_ops;
static const uint8_t* tarfs_image = 0;
static uint64_t tarfs_image_size = 0;

void tarfs_set_image(const void* data, uint64_t size) {
    // TODO: this is vulnerable to timing, either when an interrupt 
    // fires / another thread wants something so fix this
    tarfs_image = (const uint8_t*)data;
    tarfs_image_size = size;
}

static uint64_t tarfs_oct2u64(const char* s, int len) {
    uint64_t v = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; i++)
        v = (v << 3) + (uint64_t)(s[i] - '0');
    return v;
}

static uint64_t tarfs_align_up(uint64_t n) { return (n + 511ULL) & ~511ULL; }

static int tarfs_strnlen(const char* s, int max) {
    int n = 0;
    while (n < max && s[n]) n++;
    return n;
}

static int tarfs_block_is_zero(const uint8_t* b) {
    for (int i = 0; i < 512; i++) if (b[i]) return 0;
    return 1;
}

static int tarfs_strip(const char* p, int len, const char** out) {
    while (len > 0 && p[len - 1] == '/') len--;
    while (len >= 2 && p[0] == '.' && p[1] == '/') { p += 2; len -= 2; }
    while (len > 0 && p[0] == '/') { p++; len--; }
    *out = p;
    return len;
}

static void tarfs_copy_link_target(char* dst, size_t dst_size, const char* src, size_t src_size) {
    size_t len = tarfs_strnlen(src, (int)src_size);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static tarfs_node_t* vnode_to_tarfs(vnode_t* v) { return (tarfs_node_t*)v; }
static vnode_t* tarfs_to_vnode(tarfs_node_t* n) { return (vnode_t*)n; }

static tarfs_node_t* tarfs_create_node(const char* name, int len, vnode_type_t type) {
    tarfs_node_t* node = malloc(sizeof(*node));
    if (!node) return 0;
    memset(node, 0, sizeof(*node));

    node->vnode.ops = &tarfs_ops;
    node->vnode.type = type;
    node->ino = (uint64_t)(uintptr_t)node;

    if (len > 255) len = 255;
    memcpy(node->name, name, len);
    return node;
}

static tarfs_node_t* tarfs_find_child(tarfs_node_t* parent, const char* name, int len) {
    if (!parent || !name) return 0;
    for (tarfs_node_t* c = parent->children; c; c = c->next)
        if ((int)strlen(c->name) == len && memcmp(c->name, name, len) == 0)
            return c;
    return 0;
}

static void tarfs_add_child(tarfs_node_t* parent, tarfs_node_t* child) {
    if (!parent || !child) return;
    child->next = parent->children;
    parent->children = child;
    child->parent = parent;
}

static tarfs_node_t* tarfs_resolve_dir_path(tarfs_node_t* root, const char* path, int len) {
    tarfs_node_t* cur = root;

    for (int i = 0; i < len; i++) {
        int start = i;
        while (i < len && path[i] != '/') i++;
        if (i == start) continue;

        tarfs_node_t* child = tarfs_find_child(cur, path + start, i - start);
        if (!child) {
            child = tarfs_create_node(path + start, i - start, VNODE_TYPE_DIR);
            if (!child) return 0;
            tarfs_add_child(cur, child);
        }
        cur = child;
    }
    return cur;
}

static tarfs_node_t* tarfs_walk(tarfs_node_t* start, const char* path, int len, int depth);

static tarfs_node_t* tarfs_follow(tarfs_node_t* n, int depth) {
    if (!n || !n->is_symlink) return n;
    if (depth >= TARFS_MAX_SYMLINK_DEPTH) return 0;
    return tarfs_walk(n->parent, n->link_target, (int)strlen(n->link_target), depth + 1);
}

static tarfs_node_t* tarfs_walk(tarfs_node_t* start, const char* path, int len, int depth) {
    tarfs_node_t* cur = start;

    if (len > 0 && path[0] == '/')
        while (cur->parent) cur = cur->parent;

    for (int i = 0; i < len; i++) {
        int s = i;
        while (i < len && path[i] != '/') i++;
        int clen = i - s;
        if (!clen) continue;

        if (cur->vnode.type != VNODE_TYPE_DIR) return 0;

        if (clen == 1 && path[s] == '.') continue;
        if (clen == 2 && path[s] == '.' && path[s + 1] == '.') {
            if (cur->parent) cur = cur->parent;
            continue;
        }

        cur = tarfs_follow(tarfs_find_child(cur, path + s, clen), depth);
        if (!cur) return 0;
    }
    return cur;
}

static int tarfs_readdir(vnode_t* node, uint64_t index, vfs_dirent_t* entry) {
    if (!node || !entry) return -1;

    tarfs_node_t* dir = vnode_to_tarfs(node);
    if (dir->vnode.type != VNODE_TYPE_DIR) return -1;

    if (index < 2) {
        tarfs_node_t* t = (index == 1 && dir->parent) ? dir->parent : dir;
        entry->ino = t->ino;
        entry->off = index + 1;
        entry->type = 4;
        entry->name = index ? ".." : ".";
        return 0;
    }

    tarfs_node_t* child = dir->children;
    for (uint64_t n = index - 2; child && n > 0; n--) child = child->next;
    if (!child) return -1;

    tarfs_node_t* r = tarfs_follow(child, 0);
    if (!r) r = child;

    entry->ino = r->ino;
    entry->off = index + 1;
    entry->name = child->name;
    entry->type = r->vnode.type == VNODE_TYPE_DIR ? 4 : 8;
    return 0;
}

static int tarfs_lookup(vnode_t* parent, const char* name, vnode_t** result) {
    if (!parent || !name || !result) return -1;

    tarfs_node_t* p = vnode_to_tarfs(parent);
    if (p->vnode.type != VNODE_TYPE_DIR) return -1;

    tarfs_node_t* found = tarfs_follow(tarfs_find_child(p, name, (int)strlen(name)), 0);
    if (!found) return -1;

    *result = tarfs_to_vnode(found);
    return 0;
}

static int tarfs_read(vnode_t* node, void* buffer, uint64_t size, uint64_t offset, uint64_t* bytes_read) {
    if (!node || !buffer || !bytes_read) return -1;

    tarfs_node_t* n = vnode_to_tarfs(node);
    if (n->vnode.type != VNODE_TYPE_FILE) return -1;

    if (!n->data || offset >= n->vnode.size) {
        *bytes_read = 0;
        return 0;
    }

    if (offset + size > n->vnode.size) size = n->vnode.size - offset;
    memcpy(buffer, n->data + offset, size);
    *bytes_read = size;
    return 0;
}

static int tarfs_create(vnode_t* p, const char* n, vnode_t** r) { (void)p; (void)n; (void)r; return -1; }
static int tarfs_mkdir(vnode_t* p, const char* n, vnode_t** r)  { (void)p; (void)n; (void)r; return -1; }
static int tarfs_write(vnode_t* n, const void* b, uint64_t s, uint64_t o, uint64_t* w) {
    (void)n; (void)b; (void)s; (void)o; (void)w;
    return -1;
}

static vnode_ops_t tarfs_ops = {
    .lookup  = tarfs_lookup,
    .create  = tarfs_create,
    .mkdir   = tarfs_mkdir,
    .read    = tarfs_read,
    .write   = tarfs_write,
    .readdir = tarfs_readdir,
};

static int tarfs_build_path(const tar_header_t* hdr, char* out, int cap) {
    int plen = tarfs_strnlen(hdr->prefix, 155);
    int nlen = tarfs_strnlen(hdr->name, 100);
    int len = 0;

    if (plen > 0) {
        len = plen < cap ? plen : cap;
        memcpy(out, hdr->prefix, len);
        if (len < cap) out[len++] = '/';
    }

    int n = nlen < cap - len ? nlen : cap - len;
    if (n > 0) memcpy(out + len, hdr->name, n);
    return len + n;
}

static void tarfs_add_entry(tarfs_node_t* root, tarfs_node_t* parent, const char* base, int blen,
                            const tar_header_t* hdr, const uint8_t* data, uint64_t size) {
    tarfs_node_t* n = 0;

    switch (hdr->typeflag) {
    case '5':
        n = tarfs_create_node(base, blen, VNODE_TYPE_DIR);
        break;

    case '2':
        n = tarfs_create_node(base, blen, VNODE_TYPE_FILE);
        if (!n) return;
        n->is_symlink = 1;
        tarfs_copy_link_target(n->link_target, sizeof(n->link_target), hdr->linkname, sizeof(hdr->linkname));
        break;

    case '1': {
        const char* lp;
        int llen = tarfs_strip(hdr->linkname, tarfs_strnlen(hdr->linkname, 100), &lp);
        tarfs_node_t* t = tarfs_walk(root, lp, llen, 0);
        if (!t || t->vnode.type == VNODE_TYPE_DIR) return;

        n = tarfs_create_node(base, blen, t->vnode.type);
        if (!n) return;
        n->vnode.size = t->vnode.size;
        n->data = t->data;
        n->ino = t->ino;
        break;
    }

    default:
        n = tarfs_create_node(base, blen, VNODE_TYPE_FILE);
        if (!n) return;
        n->data = data;
        n->vnode.size = size;
        break;
    }

    if (n) tarfs_add_child(parent, n);
}

static void tarfs_parse_archive(tarfs_node_t* root) {
    if (!tarfs_image || tarfs_image_size < 512) return;

    uint64_t offset = 0;

    while (offset + 512 <= tarfs_image_size) {
        const tar_header_t* hdr = (const tar_header_t*)(tarfs_image + offset);
        if (tarfs_block_is_zero((const uint8_t*)hdr)) break;

        uint64_t size = tarfs_oct2u64(hdr->size, 12);
        char t = hdr->typeflag;
        offset += 512;

        if (t == '1' || t == '2') size = 0;
        if (offset + size > tarfs_image_size) break;

        if (t == 0 || t == '0' || t == '1' || t == '2' || t == '5') {
            char raw[256];
            const char* p;
            int plen = tarfs_strip(raw, tarfs_build_path(hdr, raw, sizeof(raw)), &p);

            int split = plen;
            while (split > 0 && p[split - 1] != '/') split--;

            if (plen > split) {
                tarfs_node_t* parent = tarfs_resolve_dir_path(root, p, split > 0 ? split - 1 : 0);
                if (parent && !tarfs_find_child(parent, p + split, plen - split))
                    tarfs_add_entry(root, parent, p + split, plen - split, hdr, tarfs_image + offset, size);
            }
        }

        offset += tarfs_align_up(size);
    }
}

static int tarfs_mount(filesystem_mount_t* mount, const char* source, const char* target) {
    (void)source; (void)target;
    if (!mount) return -1;

    tarfs_node_t* root = tarfs_create_node("", 0, VNODE_TYPE_DIR);
    if (!root) return -1;

    root->vnode.mount = mount;
    root->vnode.fs_private = root;
    mount->fs_private = root;
    mount->root = (vnode_t*)root;

    tarfs_parse_archive(root);
    return 0;
}

static int tarfs_unmount(filesystem_mount_t* mount) {
    (void)mount;
    return 0;
}

filesystem_driver_t tarfs_driver = {
    .name = "tarfs",
    .mount = tarfs_mount,
    .unmount = tarfs_unmount,
};
