#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR 0040000

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode)) return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

static int compare_tree_entries(const void *a, const void *b) {
    const TreeEntry *ea = (const TreeEntry *)a;
    const TreeEntry *eb = (const TreeEntry *)b;
    return strcmp(ea->name, eb->name);
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    if (!tree_out) return -1;
    tree_out->count = 0;
    if (len == 0) return 0;
    if (!data) return -1;

    const uint8_t *ptr = (const uint8_t *)data;
    const uint8_t *end = ptr + len;

    while (ptr < end) {
        if (tree_out->count >= MAX_TREE_ENTRIES) return -1;

        TreeEntry *entry = &tree_out->entries[tree_out->count];

        const uint8_t *space = memchr(ptr, ' ', end - ptr);
        if (!space) return -1;

        char mode_str[16] = {0};
        size_t mode_len = (size_t)(space - ptr);
        if (mode_len == 0 || mode_len >= sizeof(mode_str)) return -1;

        memcpy(mode_str, ptr, mode_len);
        entry->mode = (uint32_t)strtol(mode_str, NULL, 8);

        ptr = space + 1;

        const uint8_t *nul = memchr(ptr, '\0', end - ptr);
        if (!nul) return -1;

        size_t name_len = (size_t)(nul - ptr);
        if (name_len == 0 || name_len >= sizeof(entry->name)) return -1;

        memcpy(entry->name, ptr, name_len);
        entry->name[name_len] = '\0';

        ptr = nul + 1;

        if ((size_t)(end - ptr) < HASH_SIZE) return -1;
        memcpy(entry->hash.hash, ptr, HASH_SIZE);
        ptr += HASH_SIZE;

        tree_out->count++;
    }

    qsort(tree_out->entries, tree_out->count, sizeof(TreeEntry), compare_tree_entries);

    for (int i = 1; i < tree_out->count; i++) {
        if (strcmp(tree_out->entries[i - 1].name, tree_out->entries[i].name) == 0)
            return -1;
    }

    return 0;
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    if (!tree || !data_out || !len_out) return -1;

    size_t max_size = tree->count ? (size_t)tree->count * 600 : 1;
    uint8_t *buffer = malloc(max_size);
    if (!buffer) return -1;

    Tree sorted = *tree;
    qsort(sorted.entries, sorted.count, sizeof(TreeEntry), compare_tree_entries);

    size_t offset = 0;

    for (int i = 0; i < sorted.count; i++) {
        const TreeEntry *entry = &sorted.entries[i];

        int n = snprintf((char *)buffer + offset, max_size - offset,
                         "%o %s", entry->mode, entry->name);
        if (n < 0 || (size_t)n + 1 > max_size - offset) {
            free(buffer);
            return -1;
        }

        offset += (size_t)n + 1;

        if (offset + HASH_SIZE > max_size) {
            free(buffer);
            return -1;
        }

        memcpy(buffer + offset, entry->hash.hash, HASH_SIZE);
        offset += HASH_SIZE;
    }

    *data_out = buffer;
    *len_out = offset;
    return 0;
}

int tree_from_index(ObjectID *id_out) {
    if (!id_out) return -1;
    memset(id_out->hash, 0, HASH_SIZE);
    return 0;
}
