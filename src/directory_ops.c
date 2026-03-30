#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "whiteout.h"

int unionfs_readdir(const char *path,
                    void *buf,
                    fuse_fill_dir_t filler,
                    off_t offset,
                    struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Track names from upper_dir to avoid duplicates */
    char **seen = NULL;
    int seen_count = 0, seen_cap = 0;

    /* ---- PHASE 1: Scan upper_dir ---- */
    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    DIR *dp = opendir(upper_path);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0)
                continue;

            /* Never show .wh.* files to user */
            if (is_whiteout_file(de->d_name))
                continue;

            filler(buf, de->d_name, NULL, 0, 0);

            if (seen_count >= seen_cap) {
                seen_cap = seen_cap == 0 ? 16 : seen_cap * 2;
                seen = realloc(seen, seen_cap * sizeof(char *));
            }
            seen[seen_count++] = strdup(de->d_name);
        }
        closedir(dp);
    }

    /* ---- PHASE 2: Scan lower_dir ---- */
    char lower_path[MAX_PATH];
    snprintf(lower_path, MAX_PATH, "%s%s", state->lower_dir, path);

    dp = opendir(lower_path);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0)
                continue;

            if (is_whiteout_file(de->d_name))
                continue;

            /* Skip if already shown from upper_dir */
            int already_seen = 0;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen[i], de->d_name) == 0) {
                    already_seen = 1;
                    break;
                }
            }
            if (already_seen) continue;

            /* Build full fuse path to check whiteout */
            char entry_path[MAX_PATH];
            if (strcmp(path, "/") == 0)
                snprintf(entry_path, MAX_PATH, "/%s", de->d_name);
            else
                snprintf(entry_path, MAX_PATH, "%s/%s", path, de->d_name);

            /* Skip if whited-out */
            if (is_whiteout_active(entry_path, state->upper_dir))
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    for (int i = 0; i < seen_count; i++) free(seen[i]);
    free(seen);
    return 0;
}

int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    printf("MKDIR CALLED: %s\n", path);

    if (mkdir(upper_path, mode) < 0)
        return -errno;
    return 0;
}

int unionfs_rmdir(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);
    snprintf(lower_path, MAX_PATH, "%s%s", state->lower_dir, path);

    printf("RMDIR CALLED: %s\n", path);

    struct stat st;
    int upper_exists = (stat(upper_path, &st) == 0);
    int lower_exists = (stat(lower_path, &st) == 0);

    if (!upper_exists && !lower_exists)
        return -ENOENT;

    if (upper_exists) {
        if (rmdir(upper_path) < 0)
            return -errno;
    }

    if (lower_exists)
        return create_whiteout(path, state->upper_dir);

    return 0;
}

int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    char lower[MAX_PATH];
    snprintf(upper, MAX_PATH, "%s%s", state->upper_dir, path);
    snprintf(lower, MAX_PATH, "%s%s", state->lower_dir, path);

    printf("UNLINK CALLED: %s\n", path);

    /* If file exists in upper -> delete it */
    if (access(upper, F_OK) == 0)
        unlink(upper);

    /* If file exists in lower -> create whiteout to hide it */
    if (access(lower, F_OK) == 0)
        return create_whiteout(path, state->upper_dir);

    return 0;
}
