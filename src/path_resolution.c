#include "path_resolution.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int resolve_path(struct mini_unionfs_state *state,
                 const char *path,
                 char *resolved_path) {

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];
    char whiteout_path[MAX_PATH];
    char dir[MAX_PATH];

    // Upper path
    if (snprintf(upper_path, sizeof(upper_path), "%s%s",
                 state->upper_dir, path) >= sizeof(upper_path))
        return -ENAMETOOLONG;

    // Lower path
    if (snprintf(lower_path, sizeof(lower_path), "%s%s",
                 state->lower_dir, path) >= sizeof(lower_path))
        return -ENAMETOOLONG;

    // Extract filename
    const char *filename = strrchr(path, '/');
    if (filename)
        filename++;
    else
        filename = path;

    // Extract directory
    size_t dir_len = filename - path;
    if (dir_len >= sizeof(dir))
        return -ENAMETOOLONG;

    strncpy(dir, path, dir_len);
    dir[dir_len] = '\0';

    // Whiteout path
    if (snprintf(whiteout_path, sizeof(whiteout_path),
                 "%s%s/.wh.%s",
                 state->upper_dir,
                 dir,
                 filename) >= sizeof(whiteout_path))
        return -ENAMETOOLONG;

    // 1. Whiteout check
    if (access(whiteout_path, F_OK) == 0)
        return -ENOENT;

    // 2. Upper layer
    if (access(upper_path, F_OK) == 0) {
        strcpy(resolved_path, upper_path);
        return 0;
    }

    // 3. Lower layer
    if (access(lower_path, F_OK) == 0) {
        strcpy(resolved_path, lower_path);
        return 0;
    }

    // 4. Not found
    return -ENOENT;
}
