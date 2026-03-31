# Mini-UnionFS Developer Guide

## Table of Contents

1. [Overview](#overview)
2. [System Requirements](#system-requirements)
3. [Repository Layout](#repository-layout)
4. [Build Instructions](#build-instructions)
5. [Running Mini-UnionFS](#running-mini-unionfs)
6. [Test Execution Guide](#test-execution-guide)
7. [CI/CD Pipeline](#cicd-pipeline)
8. [Code Organization](#code-organization)
9. [Troubleshooting](#troubleshooting)

---

## 1. Overview

Mini-UnionFS is a user-space layered filesystem built on top of [FUSE 3](https://github.com/libfuse/libfuse).
It merges a **lower (read-only)** directory and an **upper (writable)** directory into a single unified
mount point, closely mimicking the behaviour of Linux OverlayFS (used by Docker and container runtimes).

Key behaviours:

| Operation | What happens |
|-----------|-------------|
| Read      | Upper layer is checked first; falls back to lower |
| Write     | Triggers Copy-on-Write: file copied to upper before modification |
| Delete    | Upper files removed directly; lower files hidden via whiteout marker |
| Create    | New file created directly in upper |
| List dir  | Merged listing; whiteout files are filtered out |

---

## 2. System Requirements

| Dependency | Minimum Version | How to install |
|------------|-----------------|----------------|
| GCC        | 7.0             | `sudo apt install gcc` |
| GNU Make   | 4.0             | `sudo apt install make` |
| libfuse3   | 3.0             | `sudo apt install libfuse3-dev` |
| pkg-config | any             | `sudo apt install pkg-config` |
| FUSE kernel module | 3.x    | loaded automatically on modern Linux |

Verify:

```bash
gcc --version          # GCC 7+
pkg-config --modversion fuse3   # 3.x
```

---

## 3. Repository Layout

```
mini-unionfs/
├── Makefile                    # Build system (all targets described below)
├── README.md                   # Quick-start guide
├── DESIGN.md                   # Architecture and design decisions
├── .gitignore
│
├── src/                        # Source code
│   ├── common.h                # Shared state struct and declarations
│   ├── main.c                  # FUSE initialisation and entry point
│   ├── path_resolution.c/.h    # resolve_path(): upper > lower with whiteout check
│   ├── fuse_ops_core.c         # getattr(), access()
│   ├── file_operations.c       # open(), read(), write(), create() + CoW helper
│   ├── directory_ops.c         # readdir(), mkdir(), rmdir(), unlink()
│   └── whiteout.c/.h           # Whiteout helper library
│
├── tests/                      # Test suite
│   ├── test_unionfs.sh         # Comprehensive shell integration tests
│   ├── test_integration.sh     # Cross-module integration tests
│   ├── test_cow.c              # C unit tests for CoW logic
│   └── test_whiteout.c         # C unit tests for whiteout mechanism
│
└── docs/
    └── developer_guide.md      # This document
```

---

## 4. Build Instructions

### Quick Build (release)

```bash
make
```

Produces `./mini_unionfs`.

### Debug Build

```bash
make debug
```

Enables `-g`, `-O0`, and AddressSanitizer.  Useful for debugging with gdb or valgrind.

### Available Make Targets

| Target        | Description |
|---------------|-------------|
| `make` / `make all` | Release build (optimised, no debug symbols) |
| `make release` | Same as `make all` |
| `make debug`  | Debug build with sanitisers |
| `make test`   | Build & run all tests (unit + shell) |
| `make test-unit` | Build & run C unit tests only |
| `make test-shell` | Run shell integration tests only |
| `make install` | Install binary to `$(PREFIX)/bin` (default `/usr/local/bin`) |
| `make uninstall` | Remove installed binary |
| `make clean`  | Remove object files, dependency files, and binaries |

### Custom Install Prefix

```bash
make install PREFIX=/home/user/.local
```

---

## 5. Running Mini-UnionFS

### Usage

```
./mini_unionfs <lower_dir> <upper_dir> <mountpoint>
```

### Example

```bash
mkdir -p lower upper /tmp/mnt

# Create some files in the lower layer
echo "Hello from lower" > lower/hello.txt
echo "Shared file"      > lower/shared.txt
echo "Upper override"   > upper/shared.txt

# Mount
./mini_unionfs lower upper /tmp/mnt

# Explore
ls /tmp/mnt                     # shows: hello.txt  shared.txt
cat /tmp/mnt/hello.txt          # Hello from lower
cat /tmp/mnt/shared.txt         # Upper override  (upper wins)

# Modify a lower-layer file (triggers CoW)
echo "Modified!" >> /tmp/mnt/hello.txt
ls upper/                       # hello.txt is now copied to upper
cat lower/hello.txt             # still: Hello from lower

# Delete a lower-layer file (creates whiteout)
rm /tmp/mnt/hello.txt
ls -la upper/                   # .wh.hello.txt  shared.txt

# Unmount
fusermount -u /tmp/mnt
```

### Background / Foreground

By default, `mini_unionfs` passes `-o auto_unmount` to FUSE, which daemonises the
process and automatically unmounts when the process exits.

To unmount manually:

```bash
fusermount -u <mountpoint>
# or
umount <mountpoint>
```

---

## 6. Test Execution Guide

### Run All Tests

```bash
make test
```

This runs:
1. C unit tests (`test_cow`, `test_whiteout`)
2. Shell integration tests (`tests/test_unionfs.sh`)

### C Unit Tests

The C unit tests exercise path-independent logic without requiring a FUSE mount:

```bash
# Build individually
make tests/test_cow tests/test_whiteout

# Run individually
./tests/test_cow
./tests/test_whiteout
```

Each binary exits with code 0 on success, 1 if any test fails.

### Shell Integration Tests

```bash
bash tests/test_unionfs.sh          # comprehensive single-binary tests
bash tests/test_integration.sh      # cross-module integration tests
```

The shell tests:
- Create an isolated temporary directory per test section
- Mount the FUSE filesystem
- Exercise the mounted path
- Verify layer state (lower and upper directories)
- Unmount and clean up

**Requirements:** The `mini_unionfs` binary must be built beforehand.

### Test Output Format

```
--- Section 1: Basic Layer Visibility ---
  PASSED: lower_only.txt readable
  PASSED: upper_only.txt readable
  ...

==================================================
 Test Results Summary
==================================================
  PASSED:  18
  FAILED:  0
  SKIPPED: 0
==================================================
```

---

## 7. CI/CD Pipeline

The repository includes a GitHub Actions workflow (`.github/workflows/ci.yml`) that runs on
every push and pull request to `main`.

### Workflow steps

1. **Install dependencies** – `libfuse3-dev`, `pkg-config`, `gcc`
2. **Build (release)** – `make`
3. **C unit tests** – `make test-unit`
4. **Shell tests** (if FUSE device is available) – `make test-shell`

### Local CI simulation

```bash
# Install dependencies
sudo apt install -y libfuse3-dev pkg-config gcc make

# Full CI run
make clean && make && make test-unit
```

---

## 8. Code Organization

### Data flow

```
User operation (e.g. read /foo)
    │
    ▼
FUSE kernel module
    │
    ▼
mini_unionfs process
    │
    ├─ fuse_ops_core.c   – getattr, access
    ├─ file_operations.c – open, read, write, create  ──► cow_copy() on write
    ├─ directory_ops.c   – readdir, mkdir, rmdir, unlink ──► create_whiteout()
    │
    ├─ path_resolution.c – resolve_path()
    │     1. whiteout check (upper/.wh.filename)
    │     2. upper_dir/path
    │     3. lower_dir/path
    │
    └─ whiteout.c        – helper library
          is_whiteout_file()
          is_whiteout_active()
          create_whiteout()
          make_whiteout_name()
```

### Key types

`struct mini_unionfs_state` (defined in `src/common.h`):

```c
struct mini_unionfs_state {
    char *lower_dir;   /* absolute path to read-only lower layer */
    char *upper_dir;   /* absolute path to writable upper layer  */
};
```

This struct is stored as FUSE private data and retrieved in every callback with:

```c
struct mini_unionfs_state *state =
    (struct mini_unionfs_state *) fuse_get_context()->private_data;
```

---

## 9. Troubleshooting

### `fuse.h: No such file or directory`

```bash
sudo apt install libfuse3-dev
```

### `pkg-config: command not found`

```bash
sudo apt install pkg-config
```

### Mount fails with `Transport endpoint is not connected`

The FUSE process exited unexpectedly. Check:

```bash
dmesg | tail -20       # kernel messages
fusermount -u <mnt>    # clean up stale mount
```

### `fusermount: fuse device not found` in container/CI

Some CI environments disable the FUSE kernel module. The C unit tests (`make test-unit`)
do not require FUSE and will still run.  The shell tests require a working FUSE device
(`/dev/fuse`) and `CAP_SYS_ADMIN` or user namespace permission.

### Binary produces no output on write

Ensure the upper directory exists and is writable:

```bash
ls -la upper/
chmod 755 upper/
```

### `make` error: `libfuse3-dev not found`

Install the development package and retry:

```bash
sudo apt install libfuse3-dev
make clean && make
```
