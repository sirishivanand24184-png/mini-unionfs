#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libgen.h>

#include "common.h"
#include "whiteout.h"

int is_whiteout_file(const char *filename)
{
    return strncmp(filename, WHITEOUT_PREFIX, WHITEOUT_PREFIX_LEN) == 0;
}

void make_whiteout_name(const char *filename, char *out_buf)
{
    snprintf(out_buf, MAX_PATH, "%s%s", WHITEOUT_PREFIX, filename);
}

void strip_whiteout_prefix(const char *whiteout_name, char *out_buf)
{
    strncpy(out_buf, whiteout_name + WHITEOUT_PREFIX_LEN, MAX_PATH - 1);
    out_buf[MAX_PATH - 1] = '\0';
}

int is_whiteout_active(const char *path, const char *upper_dir)
{
    char copy1[MAX_PATH], copy2[MAX_PATH];
    strncpy(copy1, path, MAX_PATH - 1);
    strncpy(copy2, path, MAX_PATH - 1);

    char *dir_part  = dirname(copy1);
    char *base_part = basename(copy2);

    char wh_name[MAX_PATH];
    make_whiteout_name(base_part, wh_name);

    /* upper_dir ≤ MAX_PATH-1, wh_name ≤ MAX_PATH-1, separator/slash ≤ 2.
     * MAX_PATH*2 guarantees no truncation for any valid input pair. */
    char wh_full[MAX_PATH * 2];
    if (strcmp(dir_part, "/") == 0 || strcmp(dir_part, ".") == 0)
        snprintf(wh_full, sizeof(wh_full), "%s/%s", upper_dir, wh_name);
    else
        snprintf(wh_full, sizeof(wh_full), "%s%s/%s", upper_dir, dir_part, wh_name);

    struct stat st;
    return (stat(wh_full, &st) == 0);
}

int create_whiteout(const char *path, const char *upper_dir)
{
    char copy1[MAX_PATH], copy2[MAX_PATH];
    strncpy(copy1, path, MAX_PATH - 1);
    strncpy(copy2, path, MAX_PATH - 1);

    char *dir_part  = dirname(copy1);
    char *base_part = basename(copy2);

    char wh_name[MAX_PATH];
    make_whiteout_name(base_part, wh_name);

    /* upper_dir ≤ MAX_PATH-1, wh_name ≤ MAX_PATH-1, separator/slash ≤ 2.
     * MAX_PATH*2 guarantees no truncation for any valid input pair. */
    char wh_full[MAX_PATH * 2];
    if (strcmp(dir_part, "/") == 0 || strcmp(dir_part, ".") == 0)
        snprintf(wh_full, sizeof(wh_full), "%s/%s", upper_dir, wh_name);
    else
        snprintf(wh_full, sizeof(wh_full), "%s%s/%s", upper_dir, dir_part, wh_name);

    int fd = open(wh_full, O_CREAT | O_WRONLY, 0644);
    if (fd < 0)
        return -errno;
    close(fd);
    return 0;
}
