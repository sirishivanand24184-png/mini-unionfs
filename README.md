# Mini-UnionFS

## Overview

Mini-UnionFS is a user-space filesystem implemented using FUSE (Filesystem in Userspace).
It simulates a layered filesystem where an **upper layer** overrides a **lower layer**, similar to how modern container systems (like Docker) manage filesystems.

This project demonstrates core filesystem concepts such as:

* Path resolution across multiple layers
* Directory merging
* File read operations
* Whiteout mechanism for file deletion

---

## Features Implemented (Member 1)

* Path Resolution (`resolve_path`)
* File Metadata Handling (`getattr`)
* File Operations (`open`, `read`)
* Directory Merging (`readdir`)
* File Deletion using Whiteout (`unlink`)
* Permission Handling (`access`)

---

## Working Principle

The filesystem operates on two directories:

* **Lower Layer** → Read-only base layer
* **Upper Layer** → Writable layer

### Behavior:

* If a file exists in both → **upper layer is used**
* If a file exists only in lower → it is read from lower
* If a file is deleted → a **whiteout file (`.wh.<filename>`)** is created in upper to hide the lower file

---

## Project Structure

```
mini-unionfs/
│
├── src/
│   ├── main.c                # FUSE operations (getattr, read, unlink, etc.)
│   ├── common.h              # Shared structures and constants
│   ├── path_resolution.c     # Core path resolution logic
│   └── path_resolution.h     # Header for path resolution
│
├── Makefile                  # Build instructions
├── README.md                 # Project documentation
├── .gitignore                # Files ignored by Git
```

---

## 🛠️ Build Instructions

```bash
make
```

---

## Run Instructions

```bash
./mini_unionfs <lower_dir> <upper_dir> <mountpoint>
```

### Example:

```bash
mkdir lower upper
echo "hello" > lower/test.txt

./mini_unionfs lower upper /tmp/mnt
```

---

## Testing

### View files:

```bash
ls /tmp/mnt
```

### Read file:

```bash
cat /tmp/mnt/test.txt
```

### Delete file (Whiteout test):

```bash
rm /tmp/mnt/test.txt
ls -la upper
```

Expected:

```
.wh.test.txt
```

---

## Key Concept: Whiteout Mechanism

When a file from the lower layer is deleted, it cannot be physically removed.
Instead, a special hidden file is created:

```
.wh.filename
```

This ensures the file is **hidden from the merged filesystem view**.

---

## Team Contribution

**Member 1 (Core Layer Implementation):**

* Path resolution logic
* FUSE initialization
* File operations (getattr, read, open)
* Directory merging
* Whiteout-based deletion

---

## Notes

* Built and tested on Ubuntu using FUSE3
* Requires `libfuse3-dev`

Install dependencies:

```bash
sudo apt install libfuse3-dev
```

---

## Status

✔ Core filesystem implementation complete
✔ Whiteout mechanism working
✔ Ready for extension and integration

---
