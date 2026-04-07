#include "common.h"
#include "path_resolution.h"

#include <fuse.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

/* ── readdir ─────────────────────────────────────────────────────────────
 * Merge upper and lower directory listings.
 * Rules:
 *  - Skip .wh.* whiteout marker files from upper.
 *  - For each lower entry, skip if upper has a matching .wh.<name> marker
 *    or if the entry already appeared from upper.
 */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];
    snprintf(upper_path, sizeof(upper_path), "%s%s", state->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", state->lower_dir, path);

    /* We track names seen from upper so lower doesn't duplicate them. */
    #define MAX_ENTRIES 4096
    #define NAME_MAX_LEN 256
    char (*seen)[NAME_MAX_LEN] = malloc(MAX_ENTRIES * sizeof(*seen));
    if (!seen)
        return -ENOMEM;
    int seen_count = 0;

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* ── Pass 1: upper directory ── */
    DIR *du = opendir(upper_path);
    if (du) {
        struct dirent *de;
        while ((de = readdir(du)) != NULL) {
            const char *name = de->d_name;

            /* skip . and .. */
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            /* skip whiteout markers – don't expose them to the user */
            if (strncmp(name, ".wh.", 4) == 0)
                continue;

            /* record name as seen */
            if (seen_count < MAX_ENTRIES) {
                snprintf(seen[seen_count], NAME_MAX_LEN, "%s", name);
                seen_count++;
            }

            filler(buf, name, NULL, 0, 0);
        }
        closedir(du);
    }

    /* ── Pass 2: lower directory ── */
    DIR *dl = opendir(lower_path);
    if (dl) {
        struct dirent *de;
        while ((de = readdir(dl)) != NULL) {
            const char *name = de->d_name;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            /* Check for whiteout marker in upper */
            char wh[MAX_PATH];
            snprintf(wh, sizeof(wh), "%s%s/.wh.%s",
                     state->upper_dir, path, name);
            if (access(wh, F_OK) == 0)
                continue;  /* hidden by whiteout */

            /* Check if already listed from upper */
            int already_seen = 0;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen[i], name) == 0) {
                    already_seen = 1;
                    break;
                }
            }
            if (already_seen)
                continue;

            filler(buf, name, NULL, 0, 0);
        }
        closedir(dl);
    }

    free(seen);
    return 0;
}

/* ── mkdir ───────────────────────────────────────────────────────────────
 * Create a new directory in the upper layer only.
 */
int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

    if (mkdir(upper, mode) == -1)
        return -errno;

    return 0;
}

/* ── rmdir ───────────────────────────────────────────────────────────────
 * Remove a directory.
 * - If the directory exists in upper, remove it.
 * - If the directory also exists in lower, create a whiteout marker so
 *   it stays hidden from future listings.
 */
int unionfs_rmdir(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    char lower[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);

    int in_upper = (access(upper, F_OK) == 0);
    int in_lower = (access(lower, F_OK) == 0);

    if (in_upper) {
        if (rmdir(upper) == -1)
            return -errno;
    } else if (!in_lower) {
        return -ENOENT;
    }

    /* If the directory came from lower (or also existed there), create a
     * whiteout marker so it is hidden from future unionfs listings. */
    if (in_lower) {
        /* Whiteout lives in the parent directory inside upper */
        char path_copy[MAX_PATH];
        char path_copy2[MAX_PATH];
        strncpy(path_copy,  path, MAX_PATH - 1);
        path_copy[MAX_PATH - 1] = '\0';
        strncpy(path_copy2, path, MAX_PATH - 1);
        path_copy2[MAX_PATH - 1] = '\0';

        char *dir_part  = dirname(path_copy);
        char *base_part = basename(path_copy2);

        char wh_path[MAX_PATH];
        if (strcmp(dir_part, "/") == 0 || strcmp(dir_part, ".") == 0)
            snprintf(wh_path, sizeof(wh_path), "%s/.wh.%s",
                     state->upper_dir, base_part);
        else
            snprintf(wh_path, sizeof(wh_path), "%s%s/.wh.%s",
                     state->upper_dir, dir_part, base_part);

        /* Create an empty whiteout marker file */
        int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (fd == -1)
            return -errno;
        close(fd);
    }

    return 0;
}

/* ── unlink ──────────────────────────────────────────────────────────────
 * Delete a file using the whiteout mechanism.
 * - If the file exists in upper, remove it.
 * - If the file exists in lower (only), or existed in lower as well,
 *   create a .wh.<filename> marker in upper to hide it.
 */
int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    char lower[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);

    int in_upper = (access(upper, F_OK) == 0);
    int in_lower = (access(lower, F_OK) == 0);

    if (!in_upper && !in_lower)
        return -ENOENT;

    if (in_upper) {
        if (unlink(upper) == -1)
            return -errno;
    }

    /* If the file exists in lower, create a whiteout so it stays hidden */
    if (in_lower) {
        char path_copy[MAX_PATH];
        char path_copy2[MAX_PATH];
        strncpy(path_copy,  path, MAX_PATH - 1);
        path_copy[MAX_PATH - 1] = '\0';
        strncpy(path_copy2, path, MAX_PATH - 1);
        path_copy2[MAX_PATH - 1] = '\0';

        char *dir_part  = dirname(path_copy);
        char *base_part = basename(path_copy2);

        char wh_path[MAX_PATH];
        if (strcmp(dir_part, "/") == 0 || strcmp(dir_part, ".") == 0)
            snprintf(wh_path, sizeof(wh_path), "%s/.wh.%s",
                     state->upper_dir, base_part);
        else
            snprintf(wh_path, sizeof(wh_path), "%s%s/.wh.%s",
                     state->upper_dir, dir_part, base_part);

        int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (fd == -1)
            return -errno;
        close(fd);
    }

    return 0;
}
