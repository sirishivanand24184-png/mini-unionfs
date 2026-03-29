#include "common.h"

#include <fuse.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    snprintf(whiteout, sizeof(whiteout),
             "%s/.wh.%s",
             state->upper_dir,
             filename);

    printf("UNLINK CALLED: %s\n", path);

    // STEP 1: If file exists in upper → delete it
    if (access(upper, F_OK) == 0) {
        unlink(upper);
    }

    // STEP 2: If file exists in lower → create whiteout
    if (access(lower, F_OK) == 0) {
        int fd = open(whiteout, O_CREAT | O_WRONLY, 0644);
        if (fd == -1)
            return -errno;
        close(fd);
    }

    return 0;
}
