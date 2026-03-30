# Mini-UnionFS

## Overview

Mini-UnionFS is a user-space filesystem implemented using FUSE (Filesystem in Userspace).
It merges two directories — an **upper (writable)** layer and a **lower (read-only)** layer — into a single unified filesystem view.

This project demonstrates how modern layered filesystems (like Docker’s OverlayFS) work internally.

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
├── Makefile
├── test_unionfs.sh
├── README.md
├── DESIGN.md
└── .gitignore
```

---

## Build Instructions

```bash
make
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
```

---

## Testing

### List files:

```bash
ls /tmp/mnt
```

### Read file:

```bash
cat /tmp/mnt/test.txt
```

### Delete file (whiteout test):

```bash
rm /tmp/mnt/test.txt
ls -la upper
```

Expected output:

```
.wh.test.txt
```

---

## Key Concept: Whiteout

Instead of deleting from the lower layer, a hidden file is created:

```
.wh.filename
```

This hides the file from the merged view.

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
---

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
How the modules connect:

- `main.c` registers all FUSE operations and passes `mini_unionfs_state`
  (lower_dir + upper_dir) as private data to every callback
- `common.h` is the single shared header — defines the state struct,
  MAX_PATH, and declares all function signatures
- Member 3's `whiteout.c` is used by both `directory_ops.c` (unlink/rmdir)
  and is the single source of truth for all whiteout logic
- `readdir()` calls `is_whiteout_active()` to hide deleted files from listing
- `unlink()` calls `create_whiteout()` instead of touching lower_dir
- All 3 automated tests pass:
  - Test 1: Layer Visibility — readdir shows merged view correctly
  - Test 2: Copy-on-Write — write to lower file copies it to upper first
  - Test 3: Whiteout — deleting a lower file creates .wh. marker in upper
 


## Requirements

```bash
sudo apt install libfuse3-dev
```

---

## Status

✔ Fully working
✔ Modular structure
✔ Whiteout implemented


---
