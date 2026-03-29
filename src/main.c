#include "common.h"
#include "path_resolution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fuse.h>

// -------------------- GETATTR --------------------
static int unionfs_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];

    int res = resolve_path(state, path, resolved);
    if (res != 0)
        return res;

    if (lstat(resolved, stbuf) == -1)
        return -errno;

    return 0;
}

// -------------------- ACCESS --------------------
static int unionfs_access(const char *path, int mask)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];

    int res = resolve_path(state, path, resolved);
    if (res != 0)
        return res;

    if (access(resolved, mask) == -1)
        return -errno;

    return 0;
}

// -------------------- OPEN --------------------
static int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];

    int res = resolve_path(state, path, resolved);
    if (res != 0)
        return res;

    int fd = open(resolved, fi->flags);
    if (fd == -1)
        return -errno;

    close(fd);
    return 0;
}

// -------------------- READ --------------------
static int unionfs_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];

    int res = resolve_path(state, path, resolved);
    if (res != 0)
        return res;

    int fd = open(resolved, O_RDONLY);
    if (fd == -1)
        return -errno;

    int bytes = pread(fd, buf, size, offset);
    if (bytes == -1)
        bytes = -errno;

    close(fd);
    return bytes;
}

// -------------------- READDIR --------------------
static int unionfs_readdir(const char *path, void *buf,
                          fuse_fill_dir_t filler,
                          off_t offset,
                          struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];

    snprintf(upper_path, sizeof(upper_path), "%s%s", state->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", state->lower_dir, path);

    DIR *dp;
    struct dirent *de;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    char seen[100][256];
    int seen_count = 0;

    // Upper layer
    dp = opendir(upper_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {

            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            if (strncmp(de->d_name, ".wh.", 4) == 0)
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
            strcpy(seen[seen_count++], de->d_name);
        }
        closedir(dp);
    }

    // Lower layer
    dp = opendir(lower_path);
    if (dp != NULL) {
        while ((de = readdir(dp)) != NULL) {

            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            int skip = 0;

            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen[i], de->d_name) == 0) {
                    skip = 1;
                    break;
                }
            }

            char wh_path[MAX_PATH];
            snprintf(wh_path, sizeof(wh_path),
                     "%s/.wh.%s",
                     state->upper_dir,
                     de->d_name);

            if (access(wh_path, F_OK) == 0)
                skip = 1;

            if (!skip)
                filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    return 0;
}

// -------------------- UNLINK --------------------
static int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];
    char whiteout_path[MAX_PATH];

    snprintf(upper_path, sizeof(upper_path), "%s%s", state->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", state->lower_dir, path);

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    snprintf(whiteout_path, sizeof(whiteout_path),
             "%s/.wh.%s",
             state->upper_dir,
             filename);

    
    // Case 1: exists in upper → delete
    if (access(upper_path, F_OK) == 0) {
        if (unlink(upper_path) == -1)
            return -errno;
        return 0;
    }

    // Case 2: exists in lower → create whiteout
    if (access(lower_path, F_OK) == 0) {
        int fd = open(whiteout_path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1)
            return -errno;

        close(fd);
        return 0;
    }

    return -ENOENT;
}

// -------------------- OPERATIONS --------------------
static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .access  = unionfs_access,
    .open    = unionfs_open,
    .read    = unionfs_read,
    .readdir = unionfs_readdir,
    .unlink  = unionfs_unlink,
};

// -------------------- MAIN --------------------
int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lower_dir> <upper_dir> <mountpoint>\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state =
        malloc(sizeof(struct mini_unionfs_state));

    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    if (!state->lower_dir || !state->upper_dir) {
        perror("realpath failed");
        exit(1);
    }

    char *fuse_argv[] = {
        argv[0],
        "-f",
        argv[3],
        NULL
    };

    int fuse_argc = 3;

    return fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);
}
