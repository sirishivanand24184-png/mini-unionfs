#include "common.h"
#include "path_resolution.h"

#include <fuse.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int unionfs_getattr(const char *path, struct stat *stbuf,
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

int unionfs_access(const char *path, int mask)
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
