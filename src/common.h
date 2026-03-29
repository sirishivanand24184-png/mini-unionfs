#ifndef COMMON_H
#define COMMON_H

#define FUSE_USE_VERSION 31

#include <fuse.h>

#define MAX_PATH 1024

struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

#endif
