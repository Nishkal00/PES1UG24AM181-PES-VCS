#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

/* PROVIDED FUNCTIONS UNCHANGED */

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    int staged_count = 0;

    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        staged_count++;
    }

    if (staged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged_count = 0;

    for (int i = 0; i < index->count; i++) {
        struct stat st;

        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged_count++;
        } else {
            if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                st.st_size != (off_t)index->entries[i].size) {
                printf("  modified:   %s\n", index->entries[i].path);
                unstaged_count++;
            }
        }
    }

    if (unstaged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    int untracked_count = 0;

    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;

        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0 ||
                strcmp(ent->d_name, ".pes") == 0 ||
                strcmp(ent->d_name, "pes") == 0 ||
                strstr(ent->d_name, ".o") != NULL)
                continue;

            int tracked = 0;

            for (int i = 0; i < index->count; i++) {
                if (strcmp(index->entries[i].path, ent->d_name) == 0) {
                    tracked = 1;
                    break;
                }
            }

            if (!tracked) {
                struct stat st;
                stat(ent->d_name, &st);

                if (S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked_count++;
                }
            }
        }

        closedir(dir);
    }

    if (untracked_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    return 0;
}

/* COMMIT 1 IMPLEMENTATION */

int index_load(Index *index) {
    if (!index) return -1;

    index->count = 0;

    FILE *fp = fopen(INDEX_FILE, "r");
    if (!fp) {
        return 0;   /* no index file yet = empty index */
    }

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry entry;
        char hex[HASH_HEX_SIZE + 1];

        int rc = fscanf(fp, "%o %64s %lu %u %511[^\n]\n",
                        &entry.mode,
                        hex,
                        &entry.mtime_sec,
                        &entry.size,
                        entry.path);

        if (rc == EOF) break;
        if (rc != 5) {
            fclose(fp);
            return -1;
        }

        if (hex_to_hash(hex, &entry.hash) != 0) {
            fclose(fp);
            return -1;
        }

        index->entries[index->count++] = entry;
    }

    fclose(fp);
    return 0;
}

int index_save(const Index *index) {
    (void)index;
    return -1;
}

int index_add(Index *index, const char *path) {
    (void)index;
    (void)path;
    return -1;
}
