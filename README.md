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
