#include "common.h"
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>

static struct fuse_operations ops = {
    .getattr = unionfs_getattr,
    .access  = unionfs_access,
    .open    = unionfs_open,
    .read    = unionfs_read,
    .readdir = unionfs_readdir,
    .unlink  = unionfs_unlink,
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

    char *args[] = { argv[0], "-f", argv[3], NULL };

    return fuse_main(3, args, &ops, state);
}
