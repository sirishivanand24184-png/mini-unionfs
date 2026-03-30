# Mini-UnionFS

## Overview

Mini-UnionFS is a user-space filesystem implemented using FUSE (Filesystem in Userspace).
It merges two directories — an **upper (writable)** layer and a **lower (read-only)** layer — into a single unified filesystem view.

This project demonstrates how modern layered filesystems (like Docker's OverlayFS) work internally.

---

## Features

* Layered filesystem (upper + lower)
* Path resolution with priority handling
* File read operations
* Directory merging
* Whiteout mechanism for file deletion
* Modular code structure
* Copy-on-Write (CoW) for lower layer files
* New file creation directly in upper layer

---

## How It Works

### Layers:

* **Lower Layer** → Base read-only files
* **Upper Layer** → Writable layer

### Rules:

* Upper layer overrides lower layer
* If file exists only in lower → it is shown
* If file is deleted → a `.wh.filename` is created in upper

---

## Project Structure

```
mini-unionfs/
├── src/
│   ├── main.c
│   ├── fuse_ops_core.c
│   ├── file_operations.c
│   ├── directory_ops.c
│   ├── whiteout.c
│   ├── path_resolution.c
│   ├── path_resolution.h
│   └── common.h
├── tests/
│   ├── test_unionfs.sh       # Comprehensive shell tests
│   ├── test_integration.sh   # Cross-module integration tests
│   ├── test_cow.c            # C unit tests for CoW
│   └── test_whiteout.c       # C unit tests for whiteout
├── docs/
│   └── developer_guide.md    # Detailed developer documentation
├── .github/
│   └── workflows/
│       └── ci.yml            # GitHub Actions CI/CD
├── Makefile
├── README.md
└── DESIGN.md
```

---

## Requirements

```bash
sudo apt install libfuse3-dev gcc make pkg-config
```

---

## Build Instructions

```bash
# Release build (default)
make

# Debug build (with AddressSanitizer)
make debug

# Clean build artifacts
make clean
```

---

## Run

```bash
./mini_unionfs <lower_dir> <upper_dir> <mountpoint>
```

### Example:

```bash
mkdir lower upper /tmp/mnt
echo "hello" > lower/test.txt

./mini_unionfs lower upper /tmp/mnt

# List files
ls /tmp/mnt          # test.txt

# Read file
cat /tmp/mnt/test.txt   # hello

# Write (triggers Copy-on-Write)
echo "world" >> /tmp/mnt/test.txt
ls upper/            # test.txt  (CoW copy)

# Delete (creates whiteout)
rm /tmp/mnt/test.txt
ls -la upper/        # .wh.test.txt

# Unmount
fusermount -u /tmp/mnt
```

---

## Testing

### Run all tests:

```bash
make test
```

### Run only C unit tests:

```bash
make test-unit
```

### Run only shell integration tests:

```bash
make test-shell
# or
bash tests/test_unionfs.sh
bash tests/test_integration.sh
```

### Test coverage:

| Test file | What it tests |
|-----------|--------------|
| `tests/test_cow.c` | CoW copy logic, permissions, edge cases |
| `tests/test_whiteout.c` | Whiteout helpers: detect, create, check active |
| `tests/test_unionfs.sh` | Full mount: visibility, CoW, whiteout, dir ops, edge cases, persistence |
| `tests/test_integration.sh` | End-to-end: read-write cycles, delete+recreate, layer merge |

---

## Key Concept: Whiteout

Instead of deleting from the lower layer, a hidden file is created:

```
.wh.filename
```

This hides the file from the merged view while leaving the lower layer intact.

---

## Documentation

See [`docs/developer_guide.md`](docs/developer_guide.md) for:
- Detailed build instructions
- System requirements
- Architecture overview
- Test execution guide
- Troubleshooting

See [`DESIGN.md`](DESIGN.md) for the full design document covering architecture,
component interactions, and design decisions.

---

## Team Contribution

**Member 1:**

* FUSE setup and initialization
* Path resolution logic
* File operations (`getattr`, `read`, `open`)
* Directory merging
* Whiteout deletion (`unlink`)
* Modular code structure

**Member 2:**
* Copy-on-Write (CoW) implementation
* `write()` operation (writes to upper only)
* `create()` operation (new files go to upper)
* CoW trigger in `open()` for write intent
* Test suite validation (all 3 tests passing)

**Member 3 (Yogitha):**
- `readdir()` — merged directory listing (upper + lower, deduped)
- `unlink()` — deletes from upper or creates whiteout for lower files
- `mkdir()` — creates new directories in upper layer
- `rmdir()` — removes directories, creates whiteout if lower copy exists
- Whiteout helper library (`whiteout.c` / `whiteout.h`):
  - `is_whiteout_file()` — detects `.wh.*` marker files
  - `is_whiteout_active()` — checks if a file is hidden by a whiteout
  - `create_whiteout()` — creates `.wh.<filename>` marker in upper dir
  - `make_whiteout_name()` — builds whiteout filename from original name

**Member 4:**
- Enhanced Makefile with debug/release builds, install/uninstall, dependency tracking, test targets
- Comprehensive shell test suite (`tests/test_unionfs.sh`) with 24+ test cases
- C unit tests for CoW (`tests/test_cow.c`) — 8 tests
- C unit tests for whiteout (`tests/test_whiteout.c`) — 15 tests
- Integration test script (`tests/test_integration.sh`)
- Developer guide (`docs/developer_guide.md`)
- Expanded DESIGN.md with architecture diagrams and flow examples
- GitHub Actions CI/CD pipeline (`.github/workflows/ci.yml`)

---

## Status

✔ Fully working
✔ Modular structure
✔ Whiteout implemented
✔ Comprehensive test suite (C unit tests + shell integration tests)
✔ CI/CD pipeline
