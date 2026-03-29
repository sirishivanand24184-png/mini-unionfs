#include "path_resolution.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int resolve_path(struct mini_unionfs_state *state,
                 const char *path,
                 char *resolved)
{
    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    // Build paths
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);

    // Extract filename
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    // Extract directory
    char dir[MAX_PATH];
    size_t dir_len = filename - path;

    if (dir_len >= sizeof(dir))
        return -ENAMETOOLONG;

    strncpy(dir, path, dir_len);
    dir[dir_len] = '\0';

    // Safe whiteout path
    if (snprintf(whiteout, sizeof(whiteout),
                 "%s%s/.wh.%s",
                 state->upper_dir,
                 dir,
                 filename) >= sizeof(whiteout))
        return -ENAMETOOLONG;

    // 🔥 WHITEOUT CHECK FIRST
    if (access(whiteout, F_OK) == 0)
        return -ENOENT;

    // Upper layer
    if (access(upper, F_OK) == 0) {
        strcpy(resolved, upper);
        return 0;
    }

    // Lower layer
    if (access(lower, F_OK) == 0) {
        strcpy(resolved, lower);
        return 0;
    }

    return -ENOENT;
}
