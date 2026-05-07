/* in-memory FAT-like filesystem */

#include "bootpack.h"
#include "memfs.h"
#include <stdio.h>
#include <string.h>

#define MEMFS_MAX_NODES 256

struct MEMFS_NODE {
    unsigned char used;
    unsigned char is_dir;
    unsigned short parent;
    unsigned short first_cluster;
    unsigned int size;
    char name[MEMFS_NAME_LEN];
};

static unsigned char *memfs_disk;
static unsigned short *memfs_fat;
static struct MEMFS_NODE memfs_nodes[MEMFS_MAX_NODES];
static int memfs_is_ready;

static int memfs_name_equal(const char *a, const char *b)
{
    int i;
    for (i = 0; i < MEMFS_NAME_LEN; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == 0) {
            return 1;
        }
    }
    return 1;
}

static void memfs_name_copy(char *dst, const char *src)
{
    int i;
    for (i = 0; i < MEMFS_NAME_LEN - 1; i++) {
        if (src[i] == 0) {
            break;
        }
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static int memfs_find_child(int parent, const char *name)
{
    int i;
    for (i = 0; i < MEMFS_MAX_NODES; i++) {
        if (memfs_nodes[i].used != 0 && memfs_nodes[i].parent == parent) {
            if (memfs_name_equal(memfs_nodes[i].name, name) != 0) {
                return i;
            }
        }
    }
    return -1;
}

static int memfs_alloc_node(void)
{
    int i;
    for (i = 1; i < MEMFS_MAX_NODES; i++) {
        if (memfs_nodes[i].used == 0) {
            memfs_nodes[i].used = 1;
            memfs_nodes[i].is_dir = 0;
            memfs_nodes[i].parent = 0;
            memfs_nodes[i].first_cluster = MEMFS_FAT_EOC;
            memfs_nodes[i].size = 0;
            memfs_nodes[i].name[0] = 0;
            return i;
        }
    }
    return -1;
}

static int memfs_alloc_cluster(void)
{
    int i;
    for (i = 0; i < MEMFS_CLUSTER_COUNT; i++) {
        if (memfs_fat[i] == MEMFS_FAT_FREE) {
            memfs_fat[i] = MEMFS_FAT_EOC;
            return i;
        }
    }
    return -1;
}

static void memfs_free_chain(unsigned short first)
{
    unsigned short cl, next;
    if (first == MEMFS_FAT_EOC) {
        return;
    }
    cl = first;
    for (;;) {
        next = memfs_fat[cl];
        memfs_fat[cl] = MEMFS_FAT_FREE;
        if (next == MEMFS_FAT_EOC) {
            break;
        }
        cl = next;
    }
}

static int memfs_resolve_path(char *path, int *parent_out, char *leaf_out)
{
    int parent, i, j, node;
    char part[MEMFS_NAME_LEN];

    if (path == 0 || path[0] == 0) {
        return -1;
    }

    parent = 0;
    i = 0;
    while (path[i] == '/') {
        i++;
    }

    if (path[i] == 0) {
        if (parent_out != 0) {
            *parent_out = 0;
        }
        if (leaf_out != 0) {
            leaf_out[0] = 0;
        }
        return 0;
    }

    for (;;) {
        j = 0;
        while (path[i] != 0 && path[i] != '/') {
            if (j >= MEMFS_NAME_LEN - 1) {
                return -2;
            }
            part[j++] = path[i++];
        }
        part[j] = 0;

        while (path[i] == '/') {
            i++;
        }

        if (path[i] == 0) {
            if (parent_out != 0) {
                *parent_out = parent;
            }
            if (leaf_out != 0) {
                memfs_name_copy(leaf_out, part);
            }
            return 0;
        }

        node = memfs_find_child(parent, part);
        if (node < 0 || memfs_nodes[node].is_dir == 0) {
            return -3;
        }
        parent = node;
    }
}

static int memfs_find_node(char *path)
{
    int parent, node, i, j;
    char part[MEMFS_NAME_LEN];

    if (path == 0 || path[0] == 0) {
        return -1;
    }

    i = 0;
    while (path[i] == '/') {
        i++;
    }
    if (path[i] == 0) {
        return 0;
    }

    parent = 0;
    for (;;) {
        j = 0;
        while (path[i] != 0 && path[i] != '/') {
            if (j >= MEMFS_NAME_LEN - 1) {
                return -1;
            }
            part[j++] = path[i++];
        }
        part[j] = 0;
        node = memfs_find_child(parent, part);
        if (node < 0) {
            return -1;
        }

        while (path[i] == '/') {
            i++;
        }
        if (path[i] == 0) {
            return node;
        }
        if (memfs_nodes[node].is_dir == 0) {
            return -1;
        }
        parent = node;
    }
}

static int memfs_write_bytes(int node, unsigned char *data, int size)
{
    int need, i, first, prev, cl, copy_size, rest;

    memfs_free_chain(memfs_nodes[node].first_cluster);
    memfs_nodes[node].first_cluster = MEMFS_FAT_EOC;
    memfs_nodes[node].size = size;

    if (size <= 0) {
        return 0;
    }

    need = (size + MEMFS_CLUSTER_SIZE - 1) / MEMFS_CLUSTER_SIZE;
    first = -1;
    prev = -1;
    for (i = 0; i < need; i++) {
        cl = memfs_alloc_cluster();
        if (cl < 0) {
            if (first >= 0) {
                memfs_free_chain((unsigned short) first);
            }
            memfs_nodes[node].first_cluster = MEMFS_FAT_EOC;
            memfs_nodes[node].size = 0;
            return -1;
        }
        if (first < 0) {
            first = cl;
        }
        if (prev >= 0) {
            memfs_fat[prev] = (unsigned short) cl;
        }
        prev = cl;
    }
    memfs_fat[prev] = MEMFS_FAT_EOC;
    memfs_nodes[node].first_cluster = (unsigned short) first;

    cl = first;
    rest = size;
    i = 0;
    while (rest > 0) {
        copy_size = rest;
        if (copy_size > MEMFS_CLUSTER_SIZE) {
            copy_size = MEMFS_CLUSTER_SIZE;
        }
        memcpy(memfs_disk + cl * MEMFS_CLUSTER_SIZE, data + i, copy_size);
        if (copy_size < MEMFS_CLUSTER_SIZE) {
            memset(memfs_disk + cl * MEMFS_CLUSTER_SIZE + copy_size, 0, MEMFS_CLUSTER_SIZE - copy_size);
        }
        rest -= copy_size;
        i += copy_size;
        if (rest > 0) {
            cl = memfs_fat[cl];
        }
    }
    return 0;
}

static int memfs_read_bytes(int node, unsigned char *out, int outmax)
{
    int cl, pos, copy_size, rest;

    if (outmax <= 0) {
        return 0;
    }
    if (memfs_nodes[node].size <= 0) {
        return 0;
    }

    rest = memfs_nodes[node].size;
    if (rest > outmax) {
        rest = outmax;
    }
    cl = memfs_nodes[node].first_cluster;
    pos = 0;
    while (rest > 0 && cl != MEMFS_FAT_EOC) {
        copy_size = rest;
        if (copy_size > MEMFS_CLUSTER_SIZE) {
            copy_size = MEMFS_CLUSTER_SIZE;
        }
        memcpy(out + pos, memfs_disk + cl * MEMFS_CLUSTER_SIZE, copy_size);
        pos += copy_size;
        rest -= copy_size;
        if (rest > 0) {
            cl = memfs_fat[cl];
        }
    }
    return pos;
}

void memfs_init(struct MEMMAN *memman)
{
    int i;

    memfs_disk = (unsigned char *) memman_alloc_4k(memman, MEMFS_CLUSTER_SIZE * MEMFS_CLUSTER_COUNT);
    memfs_fat = (unsigned short *) memman_alloc_4k(memman, sizeof(unsigned short) * MEMFS_CLUSTER_COUNT);

    if (memfs_disk == 0 || memfs_fat == 0) {
        memfs_is_ready = 0;
        return;
    }

    memset(memfs_disk, 0, MEMFS_CLUSTER_SIZE * MEMFS_CLUSTER_COUNT);
    for (i = 0; i < MEMFS_CLUSTER_COUNT; i++) {
        memfs_fat[i] = MEMFS_FAT_FREE;
    }
    memset(memfs_nodes, 0, sizeof(memfs_nodes));

    memfs_nodes[0].used = 1;
    memfs_nodes[0].is_dir = 1;
    memfs_nodes[0].parent = 0;
    memfs_nodes[0].first_cluster = MEMFS_FAT_EOC;
    memfs_nodes[0].size = 0;
    memfs_nodes[0].name[0] = '/';
    memfs_nodes[0].name[1] = 0;

    memfs_is_ready = 1;
}

int memfs_ready(void)
{
    return memfs_is_ready;
}

int memfs_mkdir(char *path)
{
    int parent, node;
    char leaf[MEMFS_NAME_LEN];

    if (memfs_is_ready == 0) {
        return -100;
    }
    if (memfs_resolve_path(path, &parent, leaf) != 0 || leaf[0] == 0) {
        return -1;
    }
    if (memfs_find_child(parent, leaf) >= 0) {
        return -2;
    }
    node = memfs_alloc_node();
    if (node < 0) {
        return -3;
    }
    memfs_nodes[node].is_dir = 1;
    memfs_nodes[node].parent = (unsigned short) parent;
    memfs_name_copy(memfs_nodes[node].name, leaf);
    return 0;
}

int memfs_create(char *path)
{
    int parent, node;
    char leaf[MEMFS_NAME_LEN];

    if (memfs_is_ready == 0) {
        return -100;
    }
    if (memfs_resolve_path(path, &parent, leaf) != 0 || leaf[0] == 0) {
        return -1;
    }
    if (memfs_find_child(parent, leaf) >= 0) {
        return -2;
    }
    node = memfs_alloc_node();
    if (node < 0) {
        return -3;
    }
    memfs_nodes[node].is_dir = 0;
    memfs_nodes[node].parent = (unsigned short) parent;
    memfs_name_copy(memfs_nodes[node].name, leaf);
    return 0;
}

int memfs_write_text(char *path, char *text)
{
    int node;
    int size;

    if (memfs_is_ready == 0) {
        return -100;
    }
    node = memfs_find_node(path);
    if (node < 0 || memfs_nodes[node].is_dir != 0) {
        return -1;
    }
    size = strlen(text);
    if (memfs_write_bytes(node, (unsigned char *) text, size) != 0) {
        return -2;
    }
    return size;
}

int memfs_read_text(char *path, char *out, int outmax)
{
    int node, n;

    if (memfs_is_ready == 0) {
        return -100;
    }
    node = memfs_find_node(path);
    if (node < 0 || memfs_nodes[node].is_dir != 0) {
        return -1;
    }
    if (outmax <= 0) {
        return -2;
    }
    n = memfs_read_bytes(node, (unsigned char *) out, outmax - 1);
    out[n] = 0;
    return n;
}

int memfs_copy(char *src_path, char *dst_path)
{
    int src, dst;
    int parent;
    char leaf[MEMFS_NAME_LEN];
    unsigned char *tmp;
    int n;
    struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

    if (memfs_is_ready == 0) {
        return -100;
    }

    src = memfs_find_node(src_path);
    if (src < 0 || memfs_nodes[src].is_dir != 0) {
        return -1;
    }
    if (memfs_resolve_path(dst_path, &parent, leaf) != 0 || leaf[0] == 0) {
        return -2;
    }
    if (memfs_find_child(parent, leaf) >= 0) {
        return -3;
    }
    dst = memfs_alloc_node();
    if (dst < 0) {
        return -4;
    }
    memfs_nodes[dst].is_dir = 0;
    memfs_nodes[dst].parent = (unsigned short) parent;
    memfs_name_copy(memfs_nodes[dst].name, leaf);

    if (memfs_nodes[src].size == 0) {
        return 0;
    }

    tmp = (unsigned char *) memman_alloc_4k(memman, memfs_nodes[src].size);
    if (tmp == 0) {
        memfs_nodes[dst].used = 0;
        return -5;
    }
    n = memfs_read_bytes(src, tmp, memfs_nodes[src].size);
    if (n != (int) memfs_nodes[src].size) {
        memman_free_4k(memman, (int) tmp, memfs_nodes[src].size);
        memfs_nodes[dst].used = 0;
        return -6;
    }
    if (memfs_write_bytes(dst, tmp, n) != 0) {
        memman_free_4k(memman, (int) tmp, memfs_nodes[src].size);
        memfs_nodes[dst].used = 0;
        return -7;
    }
    memman_free_4k(memman, (int) tmp, memfs_nodes[src].size);
    return n;
}

int memfs_remove(char *path)
{
    int node;

    if (memfs_is_ready == 0) {
        return -100;
    }
    node = memfs_find_node(path);
    if (node <= 0 || memfs_nodes[node].is_dir != 0) {
        return -1;
    }
    memfs_free_chain(memfs_nodes[node].first_cluster);
    memfs_nodes[node].used = 0;
    return 0;
}

int memfs_rmdir(char *path)
{
    int node, i;

    if (memfs_is_ready == 0) {
        return -100;
    }
    node = memfs_find_node(path);
    if (node <= 0 || memfs_nodes[node].is_dir == 0) {
        return -1;
    }
    for (i = 0; i < MEMFS_MAX_NODES; i++) {
        if (memfs_nodes[i].used != 0 && memfs_nodes[i].parent == node) {
            return -2;
        }
    }
    memfs_nodes[node].used = 0;
    return 0;
}

int memfs_ls(char *path, char *out, int outmax)
{
    int dir, i, len, n;
    char line[48];

    if (memfs_is_ready == 0) {
        return -100;
    }
    if (path == 0 || path[0] == 0) {
        dir = 0;
    } else {
        dir = memfs_find_node(path);
    }
    if (dir < 0 || memfs_nodes[dir].is_dir == 0) {
        return -1;
    }

    len = 0;
    for (i = 0; i < MEMFS_MAX_NODES; i++) {
        if (memfs_nodes[i].used != 0 && memfs_nodes[i].parent == dir && i != dir) {
            if (memfs_nodes[i].is_dir != 0) {
                sprintf(line, "<DIR> %-16s\n", memfs_nodes[i].name);
            } else {
                sprintf(line, "      %-16s %7d\n", memfs_nodes[i].name, memfs_nodes[i].size);
            }
            n = strlen(line);
            if (len + n >= outmax - 1) {
                break;
            }
            memcpy(out + len, line, n);
            len += n;
        }
    }
    out[len] = 0;
    return len;
}
