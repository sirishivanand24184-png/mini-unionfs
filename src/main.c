#include "common.h"
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>

static struct fuse_operations ops = {
    .getattr = unionfs_getattr,
    .access  = unionfs_access,
    .open    = unionfs_open,
    .read    = unionfs_read,
    .write   = unionfs_write,
    .create  = unionfs_create,
    .readdir = unionfs_readdir,
    .unlink  = unionfs_unlink,
    .mkdir   = unionfs_mkdir,
    .rmdir   = unionfs_rmdir,
};
int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Usage: %s lower upper mountpoint\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = malloc(sizeof(*state));

    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    // Pass only program name + mountpoint to FUSE (background mode)
    char *fuse_argv[] = { argv[0], "-o", "auto_unmount", argv[3], NULL };

    return fuse_main(4, fuse_argv, &ops, state);
}
