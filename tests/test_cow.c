/*
 * test_cow.c - Unit tests for Copy-on-Write operations.
 *
 * Exercises the CoW helper logic by directly manipulating lower/upper
 * directories, without mounting FUSE.  Each test is self-contained and
 * reports PASS or FAIL via stdout.
 *
 * Build:
 *   gcc -Wall -Wextra -o test_cow tests/test_cow.c
 * Run:
 *   ./test_cow
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ANSI colors */
#define GREEN "\033[0;32m"
#define RED   "\033[0;31m"
#define NC    "\033[0m"

/* Global counters */
static int pass_count = 0;
static int fail_count = 0;

/* Test infrastructure */

static void report(const char *name, int ok)
{
    if (ok) {
        printf("  " GREEN "PASSED" NC ": %s\n", name);
        pass_count++;
    } else {
        printf("  " RED "FAILED" NC ": %s\n", name);
        fail_count++;
    }
}

/* Create a temporary directory and return a malloc'd path. */
static char *make_tmpdir(void)
{
    char tmpl[] = "/tmp/test_cow_XXXXXX";
    char *d = mkdtemp(tmpl);
    return d ? strdup(d) : NULL;
}

/* Write a string to a file, creating it if needed. */
static int write_file(const char *path, const char *content)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    ssize_t n = write(fd, content, strlen(content));
    close(fd);
    return (n == (ssize_t)strlen(content)) ? 0 : -1;
}

/* Read entire file into a malloc'd buffer. Caller must free(). */
static char *read_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    fstat(fd, &st);
    char *buf = malloc((size_t)st.st_size + 1);
    if (!buf) { close(fd); return NULL; }
    ssize_t n = read(fd, buf, (size_t)st.st_size);
    close(fd);
    if (n < 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

/* Simulates the CoW copy used in file_operations.c */
static int cow_copy(const char *lower_dir, const char *upper_dir,
                    const char *rel_path)
{
    char lower[2048], upper[2048];
    snprintf(lower, sizeof(lower), "%s/%s", lower_dir, rel_path);
    snprintf(upper, sizeof(upper), "%s/%s", upper_dir, rel_path);

    int src = open(lower, O_RDONLY);
    if (src < 0) return -errno;

    struct stat st;
    fstat(src, &st);

    int dst = open(upper, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst < 0) { int e = errno; close(src); return -e; }

    char buf[4096];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0) {
        if (write(dst, buf, (size_t)n) != n) {
            close(src); close(dst);
            return -errno;
        }
    }

    close(src);
    close(dst);
    return 0;
}

/* Remove a directory recursively (best-effort). */
static void rmdir_r(const char *path)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    (void)system(cmd);
}

/* Individual tests */

static void test_cow_creates_upper_copy(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048];
    snprintf(src, sizeof(src), "%s/file.txt", lower);
    write_file(src, "original");

    int rc = cow_copy(lower, upper, "file.txt");

    char dst[2048];
    snprintf(dst, sizeof(dst), "%s/file.txt", upper);
    struct stat st;
    report("cow_copy creates file in upper layer",
           rc == 0 && stat(dst, &st) == 0);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_preserves_lower(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048];
    snprintf(src, sizeof(src), "%s/file.txt", lower);
    write_file(src, "untouched");

    cow_copy(lower, upper, "file.txt");

    char *content = read_file(src);
    report("cow_copy does not modify lower layer file",
           content && strcmp(content, "untouched") == 0);
    free(content);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_content_matches(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048], dst[2048];
    snprintf(src, sizeof(src), "%s/file.txt", lower);
    snprintf(dst, sizeof(dst), "%s/file.txt", upper);
    write_file(src, "hello cow");

    cow_copy(lower, upper, "file.txt");

    char *content = read_file(dst);
    report("cow_copy produces identical content in upper",
           content && strcmp(content, "hello cow") == 0);
    free(content);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_preserves_permissions(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048], dst[2048];
    snprintf(src, sizeof(src), "%s/perm.txt", lower);
    snprintf(dst, sizeof(dst), "%s/perm.txt", upper);
    write_file(src, "perms");
    chmod(src, 0640);

    cow_copy(lower, upper, "perm.txt");

    struct stat st;
    stat(dst, &st);
    report("cow_copy preserves file permissions",
           (st.st_mode & 0777) == 0640);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_nonexistent_source(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    int rc = cow_copy(lower, upper, "missing.txt");
    report("cow_copy returns error when source does not exist",
           rc < 0);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_empty_file(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048], dst[2048];
    snprintf(src, sizeof(src), "%s/empty.txt", lower);
    snprintf(dst, sizeof(dst), "%s/empty.txt", upper);
    write_file(src, "");

    int rc = cow_copy(lower, upper, "empty.txt");

    struct stat st;
    report("cow_copy handles empty source file",
           rc == 0 && stat(dst, &st) == 0 && st.st_size == 0);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_cow_large_file(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048], dst[2048];
    snprintf(src, sizeof(src), "%s/large.txt", lower);
    snprintf(dst, sizeof(dst), "%s/large.txt", upper);

    /* Write a ~64 KB file */
    int fd = open(src, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char block[1024];
    memset(block, 'A', sizeof(block));
    for (int i = 0; i < 64; i++)
        write(fd, block, sizeof(block));
    close(fd);

    int rc = cow_copy(lower, upper, "large.txt");

    struct stat st_s, st_d;
    stat(src, &st_s);
    stat(dst, &st_d);
    report("cow_copy handles large files correctly",
           rc == 0 && st_s.st_size == st_d.st_size);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

static void test_upper_write_does_not_touch_lower(void)
{
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    char src[2048], dst[2048];
    snprintf(src, sizeof(src), "%s/w.txt", lower);
    snprintf(dst, sizeof(dst), "%s/w.txt", upper);
    write_file(src, "old");

    cow_copy(lower, upper, "w.txt");

    /* Simulate writing to upper only */
    write_file(dst, "new");

    char *lower_content = read_file(src);
    report("writing to upper after CoW does not affect lower",
           lower_content && strcmp(lower_content, "old") == 0);
    free(lower_content);

    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}

int main(void)
{
    printf("==================================================\n");
    printf(" Mini-UnionFS CoW Unit Tests\n");
    printf("==================================================\n\n");

    printf("--- CoW: File Copy ---\n");
    test_cow_creates_upper_copy();
    test_cow_preserves_lower();
    test_cow_content_matches();
    test_cow_preserves_permissions();

    printf("\n--- CoW: Edge Cases ---\n");
    test_cow_nonexistent_source();
    test_cow_empty_file();
    test_cow_large_file();
    test_upper_write_does_not_touch_lower();

    printf("\n==================================================\n");
    printf(" Results: " GREEN "%d PASSED" NC ", " RED "%d FAILED" NC "\n",
           pass_count, fail_count);
    printf("==================================================\n");

    return (fail_count > 0) ? 1 : 0;
}
