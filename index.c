#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++)
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remain = index->count - i - 1;
            if (remain > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remain * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    return -1;
}

int index_status(const Index *index) {
    (void)index;
    return 0;
}

int index_load(Index *index) {
    if (!index) return -1;
    index->count = 0;

    FILE *fp = fopen(INDEX_FILE, "r");
    if (!fp) return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry e;
        char hex[HASH_HEX_SIZE + 1];

        int rc = fscanf(fp, "%o %64s %lu %u %511[^\n]\n",
                        &e.mode, hex, &e.mtime_sec, &e.size, e.path);

        if (rc == EOF) break;
        if (rc != 5 || hex_to_hash(hex, &e.hash) != 0) {
            fclose(fp);
            return -1;
        }

        index->entries[index->count++] = e;
    }

    fclose(fp);
    return 0;
}

static int cmp_entries(const void *a, const void *b) {
    return strcmp(((const IndexEntry *)a)->path,
                  ((const IndexEntry *)b)->path);
}

int index_save(const Index *index) {
    if (!index) return -1;

    mkdir(PES_DIR, 0755);

    char temp[] = ".pes/index.tmpXXXXXX";
    int fd = mkstemp(temp);
    if (fd < 0) return -1;

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        return -1;
    }

    Index sorted = *index;
    qsort(sorted.entries, sorted.count, sizeof(IndexEntry), cmp_entries);

    for (int i = 0; i < sorted.count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&sorted.entries[i].hash, hex);

        fprintf(fp, "%o %s %lu %u %s\n",
                sorted.entries[i].mode,
                hex,
                sorted.entries[i].mtime_sec,
                sorted.entries[i].size,
                sorted.entries[i].path);
    }

    fflush(fp);
    fsync(fd);
    fclose(fp);

    if (rename(temp, INDEX_FILE) != 0) return -1;
    return 0;
}

int index_add(Index *index, const char *path) {
    (void)index;
    (void)path;
    return -1;
}
