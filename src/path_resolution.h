#ifndef PATH_RESOLUTION_H
#define PATH_RESOLUTION_H

#include "common.h"

int resolve_path(struct mini_unionfs_state *state,
                 const char *path,
                 char *resolved_path);

#endif
