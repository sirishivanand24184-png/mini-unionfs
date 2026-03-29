#include "common.h"
#include "path_resolution.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>

int unionfs_open(const char *path, struct fuse_file_info *fi)
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

int unionfs_read(const char *path, char *buf, size_t size,
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
