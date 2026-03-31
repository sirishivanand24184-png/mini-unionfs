# Mini-UnionFS Troubleshooting Guide

## Build Errors

### `libfuse3-dev not found`

**Symptom:**
```
Makefile:10: *** "libfuse3-dev not found. Install with: sudo apt install libfuse3-dev".  Stop.
```

**Cause:** The FUSE3 development headers are not installed.

**Fix:**
```bash
sudo apt update
sudo apt install libfuse3-dev pkg-config
```

---

### `fuse.h: No such file or directory`

**Symptom:**
```
src/common.h:6:10: fatal error: fuse.h: No such file or directory
```

**Fix:**
```bash
sudo apt install libfuse3-dev
```

---

### `pkg-config: command not found`

**Symptom:**
```
/bin/sh: pkg-config: not found
```

**Fix:**
```bash
sudo apt install pkg-config
```

---

### `undefined reference to fuse_*`

**Symptom:** Linker fails with missing `fuse_main` or similar symbols.

**Fix:** Ensure `pkg-config --libs fuse3` returns `-lfuse3 -lpthread`.
```bash
pkg-config --libs fuse3
# Expected: -lfuse3 -lpthread (or -lfuse3-lite -lpthread)
```

If the output is empty, reinstall `libfuse3-dev`.

---

### Format-truncation build warnings become errors

**Symptom:**
```
error: 'snprintf' output may be truncated [-Werror=format-truncation=]
```

**Cause:** A `snprintf` destination buffer is smaller than the maximum possible
formatted string length.

**Fix:** Increase the buffer size.  In `src/whiteout.c` the internal path
buffers use `MAX_PATH * 2` to accommodate both `upper_dir` and the whiteout
name.

---

## Runtime Issues

### Mount fails with `Transport endpoint is not connected`

**Cause:** The FUSE daemon exited unexpectedly after the mount point was
created, leaving a stale mount.

**Fix:**
```bash
fusermount -u /path/to/mountpoint
# Retry:
./mini_unionfs lower upper /path/to/mountpoint
```

Check `dmesg` for kernel-level errors:
```bash
dmesg | tail -20
```

---

### `fuse: device not found, try 'modprobe fuse' first`

**Cause:** The FUSE kernel module is not loaded.

**Fix:**
```bash
sudo modprobe fuse
lsmod | grep fuse   # confirm it is loaded
```

---

### `Permission denied` when mounting

**Cause:** The user does not have permission to access `/dev/fuse`.

**Fix:**
```bash
# Add user to the fuse group
sudo usermod -aG fuse "$USER"
# Log out and back in, then retry

# Alternative: run as root (not recommended for production)
sudo ./mini_unionfs lower upper /path/to/mountpoint
```

---

### `Cannot access mountpoint` after mount

**Cause:** Upper or lower directory paths may be relative. FUSE requires
absolute paths.

**Fix:**
```bash
./mini_unionfs "$(pwd)/lower" "$(pwd)/upper" "$(pwd)/mnt"
```

---

### Writes are silently ignored

**Cause:** The upper directory does not exist or is not writable.

**Fix:**
```bash
mkdir -p upper
chmod 755 upper
ls -la upper/
```

---

### File appears deleted but whiteout not created

**Cause:** The file existed only in the upper layer; it is removed directly
(no whiteout needed).  This is correct behaviour.

**Explanation:** Whiteouts are only created when deleting a file that exists
in the lower (read-only) layer.  Deleting an upper-only file removes it
directly from `upper_dir`.

---

### Deleted file reappears after remount

**Cause:** The whiteout file was removed from the upper directory between
unmount and remount.

**Fix:** Preserve the upper directory contents between mounts.  The upper
directory is the persistent writable state; do not clear it unless you
intentionally want a fresh layer.

---

## Test Failures

### Shell tests fail with `Binary not found`

```
Binary ./mini_unionfs not found. Build first with: make
```

**Fix:**
```bash
make
bash tests/test_unionfs.sh
```

---

### Shell tests fail with `FUSE device not available`

**Cause:** No `/dev/fuse` present (common in restricted containers or CI).

**Fix:** Run only C unit tests (no FUSE required):
```bash
make test-unit
```

To enable FUSE in Docker:
```bash
docker run --device /dev/fuse --cap-add SYS_ADMIN ...
```

---

### C unit test `test_whiteout` fails to compile

**Symptom:**
```
error: '/.wh.sub.txt' directive output may be truncated [-Werror=format-truncation=]
```

**Fix:** This was a known issue fixed by increasing local path buffer sizes
in `tests/test_whiteout.c` from 4096 to 8192 bytes in the affected functions.
Ensure you have the latest version of the file.

---

### Test reports `FAILED: CoW copy not created in upper`

**Cause:** The write operation did not trigger CoW.  Possible reasons:
- The file was already in the upper directory before the test started.
- The `O_WRONLY` flag was not set when opening the file.

**Debug steps:**
```bash
# Manually trace the write path:
bash -x tests/test_unionfs.sh 2>&1 | grep -A5 "Test 2.1"
```

---

### Test reports `FAILED: whiteout created` (unexpectedly missing)

**Debug steps:**
```bash
ls -la upper/       # check for .wh.* files
ls -la lower/       # confirm original file exists in lower
```

---

## Performance Issues

### Slow directory listing

**Cause:** `readdir()` scans both layers on every call.  For directories with
many files, this is O(n) per layer.  This is by design in the current
implementation.

---

### Large file reads are slow

**Cause:** Reads go through FUSE's kernel–userspace boundary.  Performance
is lower than native filesystem reads.  This is inherent to FUSE.

---

## Stale Mounts and Cleanup

### Check for stale mounts

```bash
mount | grep mini_unionfs
# or
cat /proc/mounts | grep fuse
```

### Unmount cleanly

```bash
fusermount -u /path/to/mountpoint
```

### Force unmount (use carefully)

```bash
sudo umount -l /path/to/mountpoint   # lazy unmount
```

### Clean up after a crashed daemon

```bash
fusermount -u /path/to/mountpoint 2>/dev/null || true
rm -rf /tmp/unionfs_test_*   # remove any leftover test directories
```

---

## FUSE Daemon Crashes

### Symptoms

- Mount point becomes inaccessible (`Transport endpoint is not connected`)
- `ls mountpoint` returns `Input/output error`

### Diagnosis

```bash
dmesg | tail -20
journalctl -xe | tail -30
```

### Recovery

```bash
fusermount -u /path/to/mountpoint
./mini_unionfs lower upper /path/to/mountpoint
```

---

## Path Resolution Issues

### File exists in lower but returns `No such file or directory`

**Cause:** A whiteout marker (`upper/.wh.<filename>`) is hiding the file.

**Check:**
```bash
ls -la upper/ | grep "^.*\.wh\."
```

**Fix:** Remove the whiteout to restore visibility:
```bash
rm upper/.wh.filename
```

---

### Upper layer file not visible at mountpoint

**Cause:** The file was created in the upper directory directly (bypassing
FUSE).  FUSE may not pick it up until the daemon cache refreshes.

**Fix:** Unmount and remount:
```bash
fusermount -u mountpoint
./mini_unionfs lower upper mountpoint
```
