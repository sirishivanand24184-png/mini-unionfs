/*
 * test_whiteout.c
 * Unit tests for Team Member 3: whiteout logic
 *
 * Compile: gcc -o test_whiteout test_whiteout.c ../src/whiteout.c -I../src -DFUSE_USE_VERSION=31 `pkg-config fuse3 --cflags` `pkg-config fuse3 --libs`
 * Run:     ./test_whiteout
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "whiteout.h"

/* ---- Simple test framework ---- */
static int passed = 0;
static int failed = 0;

#define TEST(name) printf("  %-50s ", name)
#define PASS() do { printf("PASSED\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAILED (%s)\n", msg); failed++; } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

void test_is_whiteout_file()
{
    printf("\n[is_whiteout_file]\n");

    TEST(".wh.config.txt is a whiteout");
    CHECK(is_whiteout_file(".wh.config.txt") == 1, "expected 1");

    TEST(".wh.delete_me.txt is a whiteout");
    CHECK(is_whiteout_file(".wh.delete_me.txt") == 1, "expected 1");

    TEST("config.txt is NOT a whiteout");
    CHECK(is_whiteout_file("config.txt") == 0, "expected 0");

    TEST("empty string is NOT a whiteout");
    CHECK(is_whiteout_file("") == 0, "expected 0");

    TEST(".hidden is NOT a whiteout");
    CHECK(is_whiteout_file(".hidden") == 0, "expected 0");
}

void test_make_whiteout_name()
{
    printf("\n[make_whiteout_name]\n");

    char buf[2048];

    TEST("config.txt -> .wh.config.txt");
    make_whiteout_name("config.txt", buf);
    CHECK(strcmp(buf, ".wh.config.txt") == 0, buf);

    TEST("delete_me.txt -> .wh.delete_me.txt");
    make_whiteout_name("delete_me.txt", buf);
    CHECK(strcmp(buf, ".wh.delete_me.txt") == 0, buf);

    TEST("somefile -> .wh.somefile");
    make_whiteout_name("somefile", buf);
    CHECK(strcmp(buf, ".wh.somefile") == 0, buf);
}

void test_strip_whiteout_prefix()
{
    printf("\n[strip_whiteout_prefix]\n");

    char buf[2048];

    TEST(".wh.config.txt -> config.txt");
    strip_whiteout_prefix(".wh.config.txt", buf);
    CHECK(strcmp(buf, "config.txt") == 0, buf);

    TEST(".wh.delete_me.txt -> delete_me.txt");
    strip_whiteout_prefix(".wh.delete_me.txt", buf);
    CHECK(strcmp(buf, "delete_me.txt") == 0, buf);
}

void test_whiteout_on_disk()
{
    printf("\n[create_whiteout + is_whiteout_active]\n");

    /* Must be large enough for mkdtemp to write into */
    char upper[2048];
    strncpy(upper, "/tmp/test_upper_XXXXXX", sizeof(upper) - 1);

    if (mkdtemp(upper) == NULL) {
        printf("  SKIP: could not create temp dir\n");
        return;
    }

    /* Test: create whiteout for /delete_me.txt */
    TEST("create_whiteout /delete_me.txt returns 0");
    CHECK(create_whiteout("/delete_me.txt", upper) == 0, "non-zero");

    /* Verify marker file exists on disk */
    char wh_path[2048];
    snprintf(wh_path, sizeof(wh_path), "%s/.wh.delete_me.txt", upper);
    struct stat st;

    TEST("marker file exists on disk");
    CHECK(stat(wh_path, &st) == 0, "file not found");

    /* Test is_whiteout_active detects it */
    TEST("is_whiteout_active returns 1 for whited-out file");
    CHECK(is_whiteout_active("/delete_me.txt", upper) == 1, "expected 1");

    /* Test non-whited file returns 0 */
    TEST("is_whiteout_active returns 0 for normal file");
    CHECK(is_whiteout_active("/base.txt", upper) == 0, "expected 0");

    /* Test whiteout in subdirectory */
    char subdir[2048];
    snprintf(subdir, sizeof(subdir), "%s/subdir", upper);
    mkdir(subdir, 0755);

    TEST("create_whiteout /subdir/file.txt returns 0");
    CHECK(create_whiteout("/subdir/file.txt", upper) == 0, "non-zero");

    TEST("is_whiteout_active returns 1 for /subdir/file.txt");
    CHECK(is_whiteout_active("/subdir/file.txt", upper) == 1, "expected 1");

    TEST("is_whiteout_active returns 0 for /subdir/other.txt");
    CHECK(is_whiteout_active("/subdir/other.txt", upper) == 0, "expected 0");

    /* Cleanup */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", upper);
    system(cmd);
}

int main(void)
{
    printf("====================================\n");
    printf("  Mini-UnionFS Whiteout Unit Tests  \n");
    printf("  Team Member 3                     \n");
    printf("====================================\n");

    test_is_whiteout_file();
    test_make_whiteout_name();
    test_strip_whiteout_prefix();
    test_whiteout_on_disk();

    printf("\n====================================\n");
    printf("  Results: %d passed, %d failed\n", passed, failed);
    printf("====================================\n");

    return (failed > 0) ? 1 : 0;
}
