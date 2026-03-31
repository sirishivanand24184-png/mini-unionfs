# Mini-UnionFS Design Document

## 1. Introduction

Mini-UnionFS is a layered filesystem implemented using FUSE (Filesystem in Userspace).
It combines two directories into a single virtual filesystem using an upper and lower
layer model, closely mirroring Linux OverlayFS semantics used by container runtimes.

---

## 2. Objectives

* Implement a layered filesystem with upper-over-lower semantics
* Demonstrate FUSE-based filesystem design patterns
* Implement Copy-on-Write to protect the lower (read-only) layer
* Handle file deletion using the whiteout mechanism
* Provide a modular, testable codebase

---

## 3. System Architecture

```
User Process
    │  read/write/stat calls
    ▼
VFS (kernel)
    │  FUSE protocol
    ▼
mini_unionfs (userspace daemon)
    │
    ├── path_resolution    ← decides which layer to use
    ├── file_operations    ← read, write, create, open (CoW)
    ├── directory_ops      ← readdir, mkdir, rmdir, unlink
    └── whiteout           ← .wh. marker creation and detection
```

### 3.1 Upper Layer

* Writable layer for all modifications
* Stores Copy-on-Write copies of modified lower files
* Contains whiteout marker files (`.wh.<name>`) for deleted lower files
* New files and directories created here directly

### 3.2 Lower Layer

* Read-only base layer
* Never modified by Mini-UnionFS operations
* Provides the baseline set of files and directories

### 3.3 Mount Point

* Unified view presented to the user
* Upper layer takes precedence over lower layer
* Whiteout files are invisible to the user

---

## 4. Core Components

### 4.1 Global State (`common.h`)

```c
struct mini_unionfs_state {
    char *lower_dir;   /* absolute path to read-only lower layer */
    char *upper_dir;   /* absolute path to writable upper layer  */
};
```

This struct is passed as FUSE private data and retrieved in every callback via
`fuse_get_context()->private_data`.

---

### 4.2 Path Resolution (`path_resolution.c`)

`resolve_path()` determines which real filesystem path to use for a given virtual path.

#### Algorithm

```
resolve_path(path):
  1. Build whiteout path: upper_dir + dir(path) + /.wh. + basename(path)
  2. If whiteout exists → return ENOENT (file is deleted)
  3. If upper_dir/path exists → return upper path
  4. If lower_dir/path exists → return lower path
  5. Return ENOENT
```

This function is called by `getattr`, `access`, `open`, and `read`.

---

### 4.3 File Operations (`file_operations.c`)

#### `open()`

Checks if the file is opened for writing.  If so and the file only exists in the lower
layer, a Copy-on-Write copy is made before opening.

#### `read()`

Resolves the path and reads using `pread()` from the resolved layer.

#### `write()`

Always writes to the upper layer.  Triggers CoW if the file is not yet in upper.

#### `create()`

New files are created directly in the upper layer.

#### Copy-on-Write helper (`cow_copy`)

```
cow_copy(path):
  1. open lower_dir/path for reading
  2. stat() to get permissions
  3. create upper_dir/path with same permissions
  4. copy contents in 4 KB chunks
  5. close both file descriptors
```

---

### 4.4 Directory Operations (`directory_ops.c`)

#### `readdir()`

Scans both upper and lower directories and merges the listings:

1. Read all entries from upper (skip `.wh.*` markers)
2. Read all entries from lower; skip entries already in upper or hidden by whiteout
3. Return deduplicated, filtered listing

#### `mkdir()`

Creates the directory directly in the upper layer.

#### `rmdir()`

Removes the directory from upper if present; creates a whiteout marker if the
directory also exists in lower.

#### `unlink()`

Removes the file from upper if present; creates a whiteout marker if the file
exists in lower (to hide it from future listings).

---

### 4.5 Whiteout Mechanism (`whiteout.c`)

When a file in the lower layer is deleted, Mini-UnionFS cannot remove it (lower is
read-only). Instead, a hidden marker file named `.wh.<filename>` is created in the
corresponding upper directory.

#### Key functions

| Function | Purpose |
|----------|---------|
| `is_whiteout_file(name)` | Returns true if `name` starts with `.wh.` |
| `make_whiteout_name(name, buf)` | Builds `.wh.<name>` |
| `strip_whiteout_prefix(wh, buf)` | Extracts original name from whiteout |
| `is_whiteout_active(path, upper)` | Checks if a `.wh.` marker exists for path |
| `create_whiteout(path, upper)` | Creates `.wh.<name>` in upper |

#### Whiteout lifecycle

```
User deletes /foo.txt
    │
    ├── foo.txt in upper? → unlink(upper/foo.txt)
    └── foo.txt in lower? → touch(upper/.wh.foo.txt)

Next readdir:
    - upper scan: sees .wh.foo.txt → skipped
    - lower scan: sees foo.txt → is_whiteout_active? yes → skipped

Next getattr(/foo.txt):
    - resolve_path checks whiteout → found → returns ENOENT
```

---

## 5. Execution Flow Examples

### Reading a file

```
read("/doc.txt")
  resolve_path("/doc.txt")
    → check upper/.wh.doc.txt  (not found)
    → check upper/doc.txt      (not found)
    → check lower/doc.txt      (found) → return lower/doc.txt
  pread(lower/doc.txt, ...)
```

### Writing a file (CoW trigger)

```
write("/doc.txt", data)
  check upper/doc.txt          (not found)
  check lower/doc.txt          (found)
  cow_copy("/doc.txt")
    → copy lower/doc.txt → upper/doc.txt
  pwrite(upper/doc.txt, data)
```

### Deleting a lower-layer file

```
unlink("/doc.txt")
  check upper/doc.txt  → not present
  check lower/doc.txt  → present
  create_whiteout("/doc.txt", upper)
    → touch upper/.wh.doc.txt
```

---

## 6. Testing Approach

### Layer 1 — C Unit Tests

* `tests/test_cow.c` — tests CoW copy: creates file, copies, verifies content,
  permissions, lower unchanged, empty file, large file.
* `tests/test_whiteout.c` — tests whiteout helpers: `is_whiteout_file`,
  `make_whiteout_name`, `is_whiteout_active`, `create_whiteout`, round-trips.

Run with: `make test-unit`

### Layer 2 — Shell Integration Tests

* `tests/test_unionfs.sh` — mounts filesystem and exercises:
  basic visibility, CoW, whiteout, directory ops, edge cases, persistence.
* `tests/test_integration.sh` — end-to-end cross-module scenarios:
  read-write-read cycle, delete+recreate, multi-layer merge, nested paths.

Run with: `bash tests/test_unionfs.sh` or `bash tests/test_integration.sh`

### Layer 3 — CI/CD

GitHub Actions workflow builds and runs all tests on every push/pull request.

---

## 7. Design Decisions

| Decision | Rationale |
|----------|-----------|
| FUSE 3 (`FUSE_USE_VERSION 31`) | Modern API; available in Ubuntu 20.04+ |
| No kernel module | User-space is sufficient for demonstration; easier to develop/test |
| Whiteout over deletion | Preserves lower layer immutability |
| CoW on open (not on write) | Ensures file descriptor to upper is valid for subsequent writes |
| `auto_unmount` flag | Prevents stale mounts if the daemon crashes |
| Per-call path resolution | Simplest correct approach; no fd caching needed for this scope |

---

## 8. Conclusion

Mini-UnionFS successfully demonstrates layered filesystem concepts using FUSE.  The
implementation is modular, independently testable, and closely resembles real-world
systems like Linux OverlayFS and Docker's storage drivers.  The comprehensive test
suite (C unit tests + shell integration tests) validates correctness at both the
function level and the full end-to-end level.

