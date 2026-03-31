/*
 * test_whiteout.c - Unit tests for the whiteout mechanism.
 *
 * Directly calls the whiteout helper functions from src/whiteout.c
 * without mounting FUSE.  Each test is self-contained and reports
 * PASS or FAIL via stdout.
 *
 * Build:
 *   gcc -Wall -Wextra -I src $(pkg-config --cflags fuse3) \
 *       -o test_whiteout tests/test_whiteout.c src/whiteout.c
 * Run:
 *   ./test_whiteout
 */

#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "whiteout.h"

/* ANSI colors */
#define GREEN "\033[0;32m"
#define RED   "\033[0;31m"
#define NC    "\033[0m"

/* Global counters */
static int pass_count = 0;
static int fail_count = 0;

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

/* Helpers */

static char *make_tmpdir(void)
{
    char tmpl[] = "/tmp/test_wh_XXXXXX";
    char *d = mkdtemp(tmpl);
    return d ? strdup(d) : NULL;
}

static void touch(const char *path)
{
    int fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);
}

static void rmdir_r(const char *path)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    (void)system(cmd);
}

/* is_whiteout_file() */

static void test_is_whiteout_file_positive(void)
{
    report("is_whiteout_file('.wh.foo') returns true",
           is_whiteout_file(".wh.foo") != 0);
}

static void test_is_whiteout_file_negative(void)
{
    report("is_whiteout_file('foo.txt') returns false",
           is_whiteout_file("foo.txt") == 0);
}

static void test_is_whiteout_file_empty_prefix(void)
{
    report("is_whiteout_file('.wh.') returns true (empty name is still whiteout)",
           is_whiteout_file(".wh.") != 0);
}

static void test_is_whiteout_file_partial_prefix(void)
{
    report("is_whiteout_file('.wh') returns false (incomplete prefix)",
           is_whiteout_file(".wh") == 0);
}

/* make_whiteout_name() */

static void test_make_whiteout_name_basic(void)
{
    char buf[256];
    make_whiteout_name("file.txt", buf);
    report("make_whiteout_name prepends '.wh.' prefix",
           strcmp(buf, ".wh.file.txt") == 0);
}

static void test_make_whiteout_name_special_chars(void)
{
    char buf[256];
    make_whiteout_name("my file.tar.gz", buf);
    report("make_whiteout_name handles filenames with spaces/dots",
           strcmp(buf, ".wh.my file.tar.gz") == 0);
}

/* strip_whiteout_prefix() */

static void test_strip_whiteout_prefix(void)
{
    char buf[2048]; /* must match MAX_PATH used by whiteout.c */
    strip_whiteout_prefix(".wh.hello.txt", buf);
    report("strip_whiteout_prefix removes '.wh.' prefix",
           strcmp(buf, "hello.txt") == 0);
}

/* is_whiteout_active() */

static void test_is_whiteout_active_true(void)
{
    char *upper = make_tmpdir();
    char wh[2048];
    snprintf(wh, sizeof(wh), "%s/.wh.ghost.txt", upper);
    touch(wh);

    report("is_whiteout_active returns true when .wh. file exists in upper",
           is_whiteout_active("/ghost.txt", upper) != 0);

    rmdir_r(upper); free(upper);
}

static void test_is_whiteout_active_false(void)
{
    char *upper = make_tmpdir();

    report("is_whiteout_active returns false when no .wh. file exists",
           is_whiteout_active("/present.txt", upper) == 0);

    rmdir_r(upper); free(upper);
}

static void test_is_whiteout_active_subdirectory(void)
{
    char *upper = make_tmpdir();
    char sub[4096], wh[4096];
    snprintf(sub, sizeof(sub), "%s/subdir", upper);
    mkdir(sub, 0755);
    snprintf(wh, sizeof(wh), "%s/.wh.sub.txt", sub);
    touch(wh);

    report("is_whiteout_active works for files in subdirectories",
           is_whiteout_active("/subdir/sub.txt", upper) != 0);

    rmdir_r(upper); free(upper);
}

/* create_whiteout() */

static void test_create_whiteout_creates_file(void)
{
    char *upper = make_tmpdir();
    create_whiteout("/delete_me.txt", upper);

    char wh[2048];
    snprintf(wh, sizeof(wh), "%s/.wh.delete_me.txt", upper);
    struct stat st;
    report("create_whiteout creates .wh. marker file in upper",
           stat(wh, &st) == 0);

    rmdir_r(upper); free(upper);
}

static void test_create_whiteout_idempotent(void)
{
    char *upper = make_tmpdir();
    create_whiteout("/idem.txt", upper);
    int rc = create_whiteout("/idem.txt", upper);

    report("create_whiteout is idempotent (second call succeeds)",
           rc == 0);

    rmdir_r(upper); free(upper);
}

static void test_create_whiteout_subdir(void)
{
    char *upper = make_tmpdir();
    char sub[4096];
    snprintf(sub, sizeof(sub), "%s/subdir", upper);
    mkdir(sub, 0755);

    create_whiteout("/subdir/nested.txt", upper);

    char wh[4096];
    snprintf(wh, sizeof(wh), "%s/.wh.nested.txt", sub);
    struct stat st;
    report("create_whiteout works for files in subdirectories",
           stat(wh, &st) == 0);

    rmdir_r(upper); free(upper);
}

static void test_create_whiteout_fails_no_parent_dir(void)
{
    char *upper = make_tmpdir();
    /* /nonexistent_parent/ does not exist in upper */
    int rc = create_whiteout("/nonexistent_parent/file.txt", upper);
    report("create_whiteout returns error when parent dir missing",
           rc < 0);

    rmdir_r(upper); free(upper);
}

/* round-trip: create + check active */

static void test_whiteout_roundtrip(void)
{
    char *upper = make_tmpdir();
    create_whiteout("/roundtrip.txt", upper);

    report("whiteout round-trip: create then is_active returns true",
           is_whiteout_active("/roundtrip.txt", upper) != 0);

    rmdir_r(upper); free(upper);
}

int main(void)
{
    printf("==================================================\n");
    printf(" Mini-UnionFS Whiteout Unit Tests\n");
    printf("==================================================\n\n");

    printf("--- is_whiteout_file() ---\n");
    test_is_whiteout_file_positive();
    test_is_whiteout_file_negative();
    test_is_whiteout_file_empty_prefix();
    test_is_whiteout_file_partial_prefix();

    printf("\n--- make_whiteout_name() ---\n");
    test_make_whiteout_name_basic();
    test_make_whiteout_name_special_chars();

    printf("\n--- strip_whiteout_prefix() ---\n");
    test_strip_whiteout_prefix();

    printf("\n--- is_whiteout_active() ---\n");
    test_is_whiteout_active_true();
    test_is_whiteout_active_false();
    test_is_whiteout_active_subdirectory();

    printf("\n--- create_whiteout() ---\n");
    test_create_whiteout_creates_file();
    test_create_whiteout_idempotent();
    test_create_whiteout_subdir();
    test_create_whiteout_fails_no_parent_dir();

    printf("\n--- Round-trip tests ---\n");
    test_whiteout_roundtrip();

    printf("\n==================================================\n");
    printf(" Results: " GREEN "%d PASSED" NC ", " RED "%d FAILED" NC "\n",
           pass_count, fail_count);
    printf("==================================================\n");

    return (fail_count > 0) ? 1 : 0;
}
