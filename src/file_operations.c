#include "common.h"
#include "path_resolution.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* ── CoW helper ──────────────────────────────────────────────────────────
 * Copies a file from lower_dir to upper_dir so writes stay in upper.
 * Returns 0 on success, -errno on failure.
 */
static int cow_copy(struct mini_unionfs_state *state, const char *path)
{
    char lower[MAX_PATH], upper[MAX_PATH];
    snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

    /* Open source */
    int src = open(lower, O_RDONLY);
    if (src == -1) return -errno;

    /* Preserve permissions */
    struct stat st;
    fstat(src, &st);

    /* Create destination */
    int dst = open(upper, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst == -1) { close(src); return -errno; }

    /* Copy contents in chunks */
    char buf[4096];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, n) != n) {
            close(src); close(dst);
            return -errno;
        }
    }

    close(src);
    close(dst);
    printf("COW: copied %s → upper\n", path);
    return 0;
}

/* ── open ────────────────────────────────────────────────────────────────
 * If file is opened for writing and lives only in lower_dir → CoW copy.
 */
int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];
    int res = resolve_path(state, path, resolved);
    if (res != 0) return res;

    /* CoW trigger: write intent + file is still in lower */
    if (fi->flags & (O_WRONLY | O_RDWR)) {
        char upper[MAX_PATH];
        snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

        if (access(upper, F_OK) != 0) {        /* not yet in upper */
            res = cow_copy(state, path);
            if (res != 0) return res;
        }
    }

    /* Reopen with (possibly new) upper path */
    char upper[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

    /* If file ended up in upper after CoW, open that; else original */
    const char *target = (access(upper, F_OK) == 0) ? upper : resolved;

    int fd = open(target, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

/* ── read ────────────────────────────────────────────────────────────────
 * Resolve path (upper wins), then pread.
 */
int unionfs_read(const char *path, char *buf, size_t size,
                 off_t offset, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char resolved[MAX_PATH];
    int res = resolve_path(state, path, resolved);
    if (res != 0) return res;

    int fd = open(resolved, O_RDONLY);
    if (fd == -1) return -errno;

    ssize_t bytes = pread(fd, buf, size, offset);
    if (bytes == -1) bytes = -errno;

    close(fd);
    return bytes;
}

/* ── write ───────────────────────────────────────────────────────────────
 * Always writes to upper_dir. If file came from lower, CoW first.
 */
int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

    /* CoW if not yet in upper */
    if (access(upper, F_OK) != 0) {
        char lower[MAX_PATH];
        snprintf(lower, sizeof(lower), "%s%s", state->lower_dir, path);
        if (access(lower, F_OK) == 0) {
            int res = cow_copy(state, path);
            if (res != 0) return res;
        }
    }

    int fd = open(upper, O_WRONLY);
    if (fd == -1) return -errno;

    ssize_t bytes = pwrite(fd, buf, size, offset);
    if (bytes == -1) bytes = -errno;

    close(fd);
    return bytes;
}

/* ── create ──────────────────────────────────────────────────────────────
 * New files always go directly to upper_dir.
 */
int unionfs_create(const char *path, mode_t mode,
                   struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state =
        (struct mini_unionfs_state *) fuse_get_context()->private_data;

    char upper[MAX_PATH];
    snprintf(upper, sizeof(upper), "%s%s", state->upper_dir, path);

    int fd = open(upper, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd == -1) return -errno;

    close(fd);
    printf("CREATE: %s → upper\n", path);
    return 0;
}
