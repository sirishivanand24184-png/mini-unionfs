# Mini-UnionFS Design Document

## 1. Introduction

Mini-UnionFS is a layered filesystem implemented using FUSE. It combines two directories into a single virtual filesystem using an upper and lower layer model.

---

## 2. Objectives

* Implement a layered filesystem
* Understand FUSE-based filesystem design
* Implement file operations and directory merging
* Handle file deletion using whiteout mechanism

---

## 3. System Architecture

### 3.1 Upper Layer

* Writable
* Stores modified and deleted state
* Contains whiteout files

### 3.2 Lower Layer

* Read-only
* Contains base files

### 3.3 Mount Point

* Unified view exposed to the user

---

## 4. Core Components

### 4.1 Global State

`mini_unionfs_state` stores:

* upper directory path
* lower directory path

---

### 4.2 Path Resolution

The `resolve_path()` function determines file location.

#### Logic:

1. Check whiteout file
2. Check upper layer
3. Check lower layer
4. Return error if not found

---

### 4.3 File Operations

#### getattr()

Retrieves metadata using resolved path.

#### open()

Opens file from correct layer.

#### read()

Reads content using `pread()`.

---

### 4.4 Directory Operations

#### readdir()

* Reads upper and lower directories
* Filters whiteout files
* Avoids duplicates

---

### 4.5 Whiteout Mechanism

When deleting a file:

* If in upper → delete directly
* If in lower → create `.wh.filename` in upper

This hides the file without modifying lower layer.

---

## 5. Execution Flow

1. User accesses file
2. FUSE intercepts request
3. Path is resolved
4. Operation executed on correct layer
5. Result returned

---

## 6. Testing

Test cases include:

* File read
* Directory listing
* Upper override
* Whiteout deletion
* Persistence after remount

---

## 7. Conclusion

Mini-UnionFS successfully demonstrates layered filesystem concepts using FUSE. The implementation is modular, extensible, and closely resembles real-world systems like OverlayFS.

---
