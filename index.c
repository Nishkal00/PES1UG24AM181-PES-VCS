#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

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

int index_status(const Index *index) { (void)index; return 0; }

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
    mkdir(PES_DIR, 0755);

    char temp[] = ".pes/index.tmpXXXXXX";
    int fd = mkstemp(temp);
    if (fd < 0) return -1;

    FILE *fp = fdopen(fd, "w");
    if (!fp) return -1;

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
    return rename(temp, INDEX_FILE);
}

int index_add(Index *index, const char *path) {
    if (!index || !path) return -1;
    if (index->count >= MAX_INDEX_ENTRIES) return -1;

    struct stat st;
    if (stat(path, &st) != 0) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    void *buf = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    fread(buf, 1, (size_t)st.st_size, fp);
    fclose(fp);

    ObjectID id;
    if (object_write(OBJ_BLOB, buf, (size_t)st.st_size, &id) != 0) {
        free(buf);
        return -1;
    }
    free(buf);

    IndexEntry *e = index_find(index, path);
    if (!e) e = &index->entries[index->count++];

    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = id;
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size = (uint32_t)st.st_size;
    strncpy(e->path, path, sizeof(e->path) - 1);
    e->path[sizeof(e->path) - 1] = '\0';

    return index_save(index);
}
