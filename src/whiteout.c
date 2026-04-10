/*
 * whiteout.c – Whiteout mechanism helpers for Mini-UnionFS
 *
 * Team Member 3 (Yogithaas4)
 *
 * A "whiteout" is an empty marker file whose name begins with ".wh.".
 * Placing one in the upper layer hides the file of the same base name
 * in the lower layer from all unionfs operations.
 *
 * Example
 *   Deleting /config.txt from lower creates: upper_dir/.wh.config.txt
 *   Deleting /sub/foo.txt from lower creates: upper_dir/sub/.wh.foo.txt
 */

#define FUSE_USE_VERSION 31

#include "whiteout.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* MAX_PATH is defined in common.h, but whiteout.c is also compiled standalone
 * for unit tests, so fall back to a safe default when common.h is absent. */
#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

/* ── is_whiteout_file ────────────────────────────────────────────────────── */

int is_whiteout_file(const char *name)
{
    if (!name || name[0] == '\0')
        return 0;
    return strncmp(name, WH_PREFIX, WH_PREFIX_LEN) == 0 ? 1 : 0;
}

/* ── make_whiteout_name ──────────────────────────────────────────────────── */

void make_whiteout_name(const char *orig, char *buf)
{
    /* buf must be at least strlen(orig) + WH_PREFIX_LEN + 1 bytes. */
    buf[0] = '\0';
    strcat(buf, WH_PREFIX);
    strcat(buf, orig);
}

/* ── strip_whiteout_prefix ───────────────────────────────────────────────── */

void strip_whiteout_prefix(const char *whname, char *buf)
{
    /* Skip the leading ".wh." and copy the rest. */
    strcpy(buf, whname + WH_PREFIX_LEN);
}

/* ── build_whiteout_path (internal helper) ───────────────────────────────── */

/*
 * Build the real filesystem path of the whiteout marker for the given FUSE
 * path inside upper_dir, and write it into wh_path (size MAX_PATH).
 *
 * Returns 0 on success, -ENAMETOOLONG if the path would overflow.
 */
static int build_whiteout_path(const char *path, const char *upper_dir,
                               char *wh_path)
{
    /* We need two mutable copies of path because POSIX dirname/basename
     * may modify their argument. */
    char path_dir[MAX_PATH];
    char path_base[MAX_PATH];

    strncpy(path_dir,  path, MAX_PATH - 1);
    path_dir[MAX_PATH - 1]  = '\0';
    strncpy(path_base, path, MAX_PATH - 1);
    path_base[MAX_PATH - 1] = '\0';

    char *dir_part  = dirname(path_dir);
    char *base_part = basename(path_base);

    int rc;
    if (strcmp(dir_part, "/") == 0 || strcmp(dir_part, ".") == 0) {
        /* Root-level file: upper_dir/.wh.<name> */
        rc = snprintf(wh_path, MAX_PATH, "%s/.wh.%s", upper_dir, base_part);
    } else {
        /* Nested file: upper_dir/<dir>/.wh.<name> */
        rc = snprintf(wh_path, MAX_PATH, "%s%s/.wh.%s",
                      upper_dir, dir_part, base_part);
    }

    return (rc >= MAX_PATH) ? -ENAMETOOLONG : 0;
}

/* ── is_whiteout_active ──────────────────────────────────────────────────── */

int is_whiteout_active(const char *path, const char *upper_dir)
{
    char wh_path[MAX_PATH];

    if (build_whiteout_path(path, upper_dir, wh_path) != 0)
        return 0;

    return (access(wh_path, F_OK) == 0) ? 1 : 0;
}

/* ── create_whiteout ─────────────────────────────────────────────────────── */

int create_whiteout(const char *path, const char *upper_dir)
{
    char wh_path[MAX_PATH];

    int rc = build_whiteout_path(path, upper_dir, wh_path);
    if (rc != 0)
        return rc;

    /* Create an empty marker file (truncate if it already exists). */
    int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd == -1)
        return -errno;
    close(fd);

    return 0;
}
