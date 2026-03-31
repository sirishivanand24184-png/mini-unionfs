# Mini-UnionFS Testing Guide

## Overview

The test suite has three layers:

| Layer | Files | Requires FUSE? |
|-------|-------|----------------|
| C unit tests | `tests/test_cow.c`, `tests/test_whiteout.c` | No |
| Shell integration tests | `tests/test_unionfs.sh` | Yes |
| End-to-end integration | `tests/test_integration.sh` | Yes |

---

## Running the Tests

### Run all tests

```bash
make test-all
```

### Run only C unit tests (no FUSE device needed)

```bash
make test-unit
```

### Run only shell integration tests

```bash
make test-shell
# or directly:
bash tests/test_unionfs.sh
```

### Run end-to-end integration tests

```bash
make test-integration
# or directly:
bash tests/test_integration.sh
```

---

## C Unit Tests

### `tests/test_cow.c` — Copy-on-Write unit tests

Tests the CoW copy helper directly without mounting FUSE.

| Test | Description |
|------|-------------|
| `cow_copy creates file in upper layer` | Basic copy from lower to upper |
| `cow_copy does not modify lower layer file` | Lower stays untouched |
| `cow_copy produces identical content in upper` | Content byte-for-byte match |
| `cow_copy preserves file permissions` | `chmod` mode copied |
| `cow_copy returns error when source does not exist` | Proper ENOENT handling |
| `cow_copy handles empty source file` | Zero-byte file edge case |
| `cow_copy handles large files correctly` | 64 KB file copied correctly |
| `writing to upper after CoW does not affect lower` | Post-CoW isolation |

Build and run manually:

```bash
gcc -Wall -Wextra -Werror -o /tmp/test_cow tests/test_cow.c
/tmp/test_cow
```

### `tests/test_whiteout.c` — Whiteout unit tests

Tests the whiteout helper library (`src/whiteout.c`) without mounting FUSE.

| Test | Description |
|------|-------------|
| `is_whiteout_file('.wh.foo')` | Positive detection |
| `is_whiteout_file('foo.txt')` | Negative detection |
| `is_whiteout_file('.wh.')` | Empty name edge case |
| `is_whiteout_file('.wh')` | Incomplete prefix |
| `make_whiteout_name` basic | Prepends `.wh.` prefix |
| `make_whiteout_name` special chars | Spaces and dots in name |
| `strip_whiteout_prefix` | Extracts original filename |
| `is_whiteout_active` true | Marker file present in upper |
| `is_whiteout_active` false | No marker file present |
| `is_whiteout_active` subdirectory | Works in nested directories |
| `create_whiteout` creates file | Marker file created |
| `create_whiteout` idempotent | Second call succeeds |
| `create_whiteout` subdirectory | Nested path support |
| `create_whiteout` missing parent | Returns error |
| Whiteout round-trip | Create then detect |

Build and run manually:

```bash
FUSE_CFLAGS=$(pkg-config --cflags fuse3)
gcc -Wall -Wextra -Werror -I src $FUSE_CFLAGS \
    -o /tmp/test_whiteout tests/test_whiteout.c src/whiteout.c
/tmp/test_whiteout
```

---

## Shell Integration Tests (`tests/test_unionfs.sh`)

Mounts the filesystem and exercises real POSIX operations.

### Sections

| Section | Tests | Description |
|---------|-------|-------------|
| 1. Basic Layer Visibility | 1.1–1.4 | Files from both layers visible, upper wins |
| 2. Copy-on-Write | 2.1–2.5 | CoW trigger, lower intact, new file in upper |
| 3. Whiteout Mechanism | 3.1–3.5 | Deletion, hiding, lower preserved |
| 4. Directory Operations | 4.1–4.4 | Subdirs, mkdir, merged listing, no duplicates |
| 5. Edge Cases | 5.1–5.6 | ENOENT, empty mount, overwrite, pre-existing whiteout |
| 6. Persistence After Remount | 6.1–6.2 | CoW and whiteout survive unmount/remount |

### Output format

```
--- Section 1: Basic Layer Visibility ---
  PASSED: lower_only.txt readable
  PASSED: upper_only.txt readable
  ...
==================================================
 Test Results Summary
==================================================
  PASSED:  22
  FAILED:  0
  SKIPPED: 0
```

---

## End-to-End Integration Tests (`tests/test_integration.sh`)

Exercises cross-module interactions.

| Integration test | Description |
|-----------------|-------------|
| 1. Read-Write-Read Cycle | Read → append → read back |
| 2. Delete and Recreate Cycle | Delete → whiteout → recreate → verify |
| 3. Multi-Layer Directory Merge | Shared dir, both-layer files, upper wins |
| 4. Whiteout Coexistence | Delete one file, create another, check listing |
| 5. Nested Path Operations | Deep directory CoW trigger |
| 6. C Unit Tests | Compiles and runs test_whiteout and test_cow |

---

## Understanding Test Output

### PASSED / FAILED / SKIPPED

- **PASSED** (green): assertion held, behaviour correct
- **FAILED** (red): assertion failed — check the test description and your implementation
- **SKIPPED** (yellow): test omitted because a prerequisite was unavailable (e.g. FUSE device)

### Exit codes

| Exit code | Meaning |
|-----------|---------|
| 0         | All tests passed |
| 1         | One or more tests failed |

---

## Adding New Tests

### C unit test template

```c
static void test_my_feature(void)
{
    /* 1. Set up isolated temp directories */
    char *lower = make_tmpdir();
    char *upper = make_tmpdir();

    /* 2. Exercise the feature */
    /* ... */

    /* 3. Assert the expected outcome */
    report("description of what should pass", /* condition */ 1 == 1);

    /* 4. Clean up */
    rmdir_r(lower); free(lower);
    rmdir_r(upper); free(upper);
}
```

Add the call to `main()` in the appropriate section.

### Shell test template

```bash
echo -n "Test N.M: Brief description... "
setup    # creates $TEST_DIR, $LOWER_DIR, $UPPER_DIR, $MOUNT_DIR
# populate lower/upper
mount_fs # mounts via $FUSE_BINARY

if <condition>; then
    pass "description"
else
    fail "description"
fi

teardown # unmounts and removes $TEST_DIR
```

---

## Coverage Analysis

```bash
make coverage
```

This builds the unit test binaries with `--coverage` (gcov) and runs them.
Coverage data (`.gcda` files) and source annotations (`.gcov` files) are written
to the repository root.

View coverage per function:

```bash
gcov src/whiteout.c
cat whiteout.c.gcov | head -50
```

---

## Debugging Failed Tests

### C unit test failure

1. Build with debug symbols:
   ```bash
   make debug
   ```
2. Run the failing test binary under GDB or AddressSanitizer:
   ```bash
   make test-unit
   # If a test prints FAILED, look at the test function name and check the assertion
   ```

### Shell test failure

1. Run with `bash -x` for verbose output:
   ```bash
   bash -x tests/test_unionfs.sh 2>&1 | head -80
   ```
2. Check FUSE device availability:
   ```bash
   ls -la /dev/fuse
   id          # ensure you are in the 'fuse' group or using sudo
   ```
3. Check for stale mounts:
   ```bash
   mount | grep fuse
   fusermount -u /tmp/mnt   # unmount stale
   ```

---

## CI/CD Pipeline

The GitHub Actions workflow (`.github/workflows/ci.yml`) runs on every push
and pull request to `main`:

1. Install `libfuse3-dev`, `pkg-config`, `gcc`, `make`
2. Build release binary (`make`)
3. Run C unit tests (`make test-unit`)
4. Run shell tests if `/dev/fuse` is available (`make test-shell`)
5. Build debug binary (`make debug`)

C unit tests always run in CI.  Shell tests require a FUSE-capable environment
and are skipped automatically when `/dev/fuse` is absent.
