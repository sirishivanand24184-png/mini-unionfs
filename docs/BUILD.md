# Mini-UnionFS Build Instructions

## System Requirements

| Component | Version | Notes |
|-----------|---------|-------|
| OS        | Ubuntu 22.04 LTS (or compatible) | FUSE kernel module required |
| GCC       | ≥ 9.0  | C99 with GNU extensions |
| Make      | ≥ 4.0  | GNU Make |
| FUSE 3    | ≥ 3.9  | `libfuse3-dev` package |
| pkg-config | any   | Detects FUSE compile/link flags |

---

## Install Dependencies

```bash
sudo apt update
sudo apt install -y libfuse3-dev gcc make pkg-config
```

On Fedora / RHEL-based systems:

```bash
sudo dnf install fuse3-devel gcc make pkgconf-pkg-config
```

---

## Step-by-Step Build

### 1. Clone the repository

```bash
git clone <repo-url> mini-unionfs
cd mini-unionfs
```

### 2. Verify dependencies

```bash
pkg-config --modversion fuse3   # should print 3.x.x
gcc --version                   # should print 9+
```

### 3. Release build (default)

```bash
make
```

Produces the binary `./mini_unionfs`.

### 4. Debug build

```bash
make debug
```

Enables `-g`, `-O0`, and AddressSanitizer.  Useful with `gdb` or `valgrind`.

### 5. Clean build artefacts

```bash
make clean
```

---

## Available Make Targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Release build (optimised, `-O2`) |
| `make release`      | Same as `make all` |
| `make debug`        | Debug build with AddressSanitizer |
| `make test`         | C unit tests + shell integration tests |
| `make test-unit`    | C unit tests only (no FUSE device required) |
| `make test-shell`   | Shell integration tests (FUSE device required) |
| `make test-integration` | End-to-end integration tests (FUSE required) |
| `make test-all`     | All test suites |
| `make coverage`     | Build with gcov and run unit tests |
| `make install`      | Install binary to `$(PREFIX)/bin` (default `/usr/local/bin`) |
| `make uninstall`    | Remove the installed binary |
| `make clean`        | Remove object files, dependency files, and binaries |
| `make help`         | Display all available targets |

---

## Installation

```bash
# Install to /usr/local/bin (default)
sudo make install

# Install to a custom prefix
make install PREFIX=/opt/mini-unionfs
```

## Uninstallation

```bash
sudo make uninstall
```

---

## Build Customisation

Override compiler or prefix at build time:

```bash
make CC=clang
make PREFIX=/usr/local
make DESTDIR=/staging install
```

---

## Common Build Errors

### `libfuse3-dev not found`

```
Makefile:10: *** "libfuse3-dev not found. Install with: sudo apt install libfuse3-dev"
```

**Fix:**
```bash
sudo apt install libfuse3-dev
```

### `pkg-config: command not found`

**Fix:**
```bash
sudo apt install pkg-config
```

### `fuse.h: No such file or directory`

This happens when `libfuse3-dev` is installed but `pkg-config` cannot locate it.

**Fix:**
```bash
sudo apt install --reinstall libfuse3-dev
pkg-config --cflags fuse3
```

### Linker error: `undefined reference to fuse_*`

Ensure the FUSE library is linked.  The Makefile uses `$(shell pkg-config --libs fuse3)`
which expands to `-lfuse3 -lpthread`.

**Manual check:**
```bash
pkg-config --libs fuse3
# should print: -lfuse3 -lpthread  (or similar)
```

### Build warnings treated as errors on main sources

The main source files (Members 1–3) may emit warnings with some compiler versions.
Build with relaxed flags for those files only:

```bash
# Remove -Werror from COMMON_CFLAGS in Makefile if needed for main sources
```

The C unit tests (`test_cow.c`, `test_whiteout.c`) are always built with
`-Wall -Wextra -Werror`.

---

## Debug vs Release Builds

| Feature | `make` (release) | `make debug` |
|---------|-----------------|--------------|
| Optimisation | `-O2` | `-O0` |
| Debug symbols | No | `-g` |
| AddressSanitizer | No | `-fsanitize=address` |
| Assertions | Disabled (`-DNDEBUG`) | Enabled |
| Use case | Production / CI | Development / debugging |

---

## Build Output

```
mini-unionfs/
├── mini_unionfs          ← main FUSE binary
├── tests/
│   ├── test_cow          ← CoW unit test binary
│   └── test_whiteout     ← whiteout unit test binary
└── src/
    └── *.o, *.d          ← object and dependency files
```
