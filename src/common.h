#ifndef COMMON_H
#define COMMON_H

#define FUSE_USE_VERSION 31   // VERY IMPORTANT

#include <fuse.h>             // gives fuse_file_info, fuse_fill_dir_t
#include <sys/stat.h>         // struct stat
#include <stddef.h>           // size_t
#include <unistd.h>           // off_t

#define MAX_PATH 1024

struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

// core ops
int unionfs_getattr(const char *, struct stat *, struct fuse_file_info *);
int unionfs_access(const char *, int);

// file ops
int unionfs_open(const char *, struct fuse_file_info *);
int unionfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);

// dir ops
int unionfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
                    struct fuse_file_info *, enum fuse_readdir_flags);

// whiteout
int unionfs_unlink(const char *);

#endif
