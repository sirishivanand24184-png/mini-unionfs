# Mini-UnionFS Design Document

**Course:** Cloud Computing
**Module:** FUSE-based Filesystem Implementation
**Team Member:** Member 1

---

## 1. Introduction

Mini-UnionFS is a user-space filesystem built using FUSE (Filesystem in Userspace) that simulates a layered filesystem architecture. It combines two directories — an **upper (writable) layer** and a **lower (read-only) layer** — into a single unified view.

This design is inspired by modern container storage systems such as Docker’s OverlayFS, where multiple layers are merged to provide efficient file access and modification.

---

## 2. Objectives

The primary objectives of this implementation are:

* To understand filesystem abstraction using FUSE
* To implement layered file access using upper and lower directories
* To design efficient path resolution logic
* To support file metadata and read operations
* To implement a whiteout mechanism for file deletion

---

## 3. System Architecture

The system consists of three main components:

### 3.1 Upper Layer

* Writable directory
* Stores modified and newly created files
* Contains whiteout files for deletions

### 3.2 Lower Layer

* Read-only directory
* Contains the base set of files

### 3.3 Mount Point

* The merged virtual filesystem exposed to the user
* All operations are performed through this interface

---

## 4. Core Design Components

### 4.1 Global State Management

A structure `mini_unionfs_state` is used to maintain:

* Path to upper directory
* Path to lower directory

This state is passed to all FUSE operations using `fuse_get_context()`.

---

### 4.2 Path Resolution Mechanism

The `resolve_path()` function is the core of the filesystem.

#### Logic:

1. Construct path in upper layer
2. Check if a corresponding whiteout file exists
3. If whiteout exists → return `-ENOENT`
4. If file exists in upper → return upper path
5. Else if file exists in lower → return lower path
6. Otherwise → return `-ENOENT`

This ensures correct precedence and file visibility.

---

### 4.3 File Operations

#### `getattr()`

* Retrieves metadata of files
* Uses resolved path
* Ensures correct file attributes

#### `open()`

* Opens file from resolved path
* Ensures file accessibility

#### `read()`

* Reads file content from appropriate layer
* Uses `pread()` for offset-based reading

---

### 4.4 Directory Merging (`readdir`)

Directory entries from both layers are merged:

* Upper layer entries are listed first
* Duplicate entries from lower are ignored
* Whiteout files are excluded
* Hidden files (`.wh.*`) are filtered

---

### 4.5 Whiteout Mechanism (Deletion Handling)

Deletion is implemented using a whiteout strategy.

#### Behavior:

* If file exists in upper → directly deleted
* If file exists only in lower → create `.wh.filename` in upper

#### Purpose:

* Prevents modification of lower (read-only) layer
* Hides file from merged view

---

### 4.6 Access Control (`access`)

The `access()` function ensures:

* Permission checks before operations
* Proper interaction with FUSE
* Enables correct execution of operations like `unlink`

---

## 5. File Structure

```
mini-unionfs/
│
├── src/
│   ├── main.c                # FUSE operations
│   ├── common.h              # Shared definitions
│   ├── path_resolution.c     # Path resolution logic
│   └── path_resolution.h
│
├── Makefile
├── README.md
├── .gitignore
```

---

## 6. Execution Flow

1. User accesses file via mount point
2. FUSE intercepts system call
3. Corresponding operation (e.g., `read`, `unlink`) is triggered
4. `resolve_path()` determines actual file location
5. Operation is performed on upper or lower layer
6. Result is returned to user

---

## 7. Testing and Validation

The system was tested using:

* File read operations
* Directory listing
* Deletion using whiteout
* Visibility checks across layers

### Example:

```
echo "hello" > lower/test.txt
./mini_unionfs lower upper /tmp/mnt
rm /tmp/mnt/test.txt
```

Result:

```
upper/.wh.test.txt
```

---

## 8. Conclusion

The Mini-UnionFS successfully demonstrates the implementation of a layered filesystem using FUSE. It provides a clear understanding of path resolution, file abstraction, and deletion mechanisms using whiteout files.

The system is modular, extensible, and serves as a strong foundation for further enhancements such as CoW and advanced filesystem operations.

---
