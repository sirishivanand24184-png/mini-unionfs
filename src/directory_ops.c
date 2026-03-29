#include "common.h"
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>   // 🔥 FIXED

int unionfs_readdir(const char *path, void *buf,
                    fuse_fill_dir_t filler,
                    off_t offset,
                    struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    char lower[MAX_PATH];
    char whiteout[MAX_PATH];

    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    DIR *dp;
    struct dirent *de;

    // 🔹 Upper layer first
    dp = opendir(upper);
    if (dp) {
        while ((de = readdir(dp))) {
            if (strncmp(de->d_name, ".wh.", 4) == 0)
                continue;

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    // 🔹 Lower layer (skip whiteouted files)
    dp = opendir(lower);
    if (dp) {
        while ((de = readdir(dp))) {

            if (snprintf(whiteout, sizeof(whiteout),
                         "%s%s/.wh.%s",
                         state->upper_dir,
                         path,
                         de->d_name) >= sizeof(whiteout))
                continue;

            if (access(whiteout, F_OK) == 0)
                continue;  // 🔥 hide file

            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    return 0;
}
