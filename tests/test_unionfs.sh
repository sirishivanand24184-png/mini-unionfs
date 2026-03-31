#!/bin/bash
# Comprehensive Mini-UnionFS Shell Test Suite
# Tests: visibility, CoW, whiteout, directory ops, edge cases, permissions

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FUSE_BINARY="$REPO_DIR/mini_unionfs"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

pass() { echo -e "  ${GREEN}PASSED${NC}: $1"; PASS_COUNT=$((PASS_COUNT + 1)); }
fail() { echo -e "  ${RED}FAILED${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT + 1)); }
skip() { echo -e "  ${YELLOW}SKIPPED${NC}: $1"; SKIP_COUNT=$((SKIP_COUNT + 1)); }

# helpers

setup() {
    TEST_DIR="$(mktemp -d)"
    LOWER_DIR="$TEST_DIR/lower"
    UPPER_DIR="$TEST_DIR/upper"
    MOUNT_DIR="$TEST_DIR/mnt"
    mkdir -p "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
}

teardown() {
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null || true
    rm -rf "$TEST_DIR"
}

mount_fs() {
    "$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
    sleep 1
}

# prerequisite check

echo "=================================================="
echo " Mini-UnionFS Comprehensive Test Suite"
echo "=================================================="
echo ""

if [ ! -f "$FUSE_BINARY" ]; then
    echo -e "${RED}Binary $FUSE_BINARY not found. Build first with: make${NC}"
    exit 1
fi

# SECTION 1: Basic Visibility

echo "--- Section 1: Basic Layer Visibility ---"

setup
echo "lower_content" > "$LOWER_DIR/lower_only.txt"
echo "upper_content" > "$UPPER_DIR/upper_only.txt"
mount_fs

echo -n "Test 1.1: Lower-layer file visible in mount... "
if grep -q "lower_content" "$MOUNT_DIR/lower_only.txt" 2>/dev/null; then
    pass "lower_only.txt readable"
else
    fail "lower_only.txt not readable"
fi

echo -n "Test 1.2: Upper-layer file visible in mount... "
if grep -q "upper_content" "$MOUNT_DIR/upper_only.txt" 2>/dev/null; then
    pass "upper_only.txt readable"
else
    fail "upper_only.txt not readable"
fi

echo -n "Test 1.3: Upper layer overrides lower layer... "
echo "lower_val" > "$LOWER_DIR/shared.txt"
echo "upper_val" > "$UPPER_DIR/shared.txt"
if grep -q "upper_val" "$MOUNT_DIR/shared.txt" 2>/dev/null; then
    pass "upper overrides lower"
else
    fail "upper did not override lower"
fi

echo -n "Test 1.4: Directory listing shows files from both layers... "
FILES=$(ls "$MOUNT_DIR" 2>/dev/null)
if echo "$FILES" | grep -q "lower_only.txt" && \
   echo "$FILES" | grep -q "upper_only.txt"; then
    pass "readdir shows merged listing"
else
    fail "readdir missing files (got: $FILES)"
fi

teardown
echo ""

# SECTION 2: Copy-on-Write

echo "--- Section 2: Copy-on-Write (CoW) ---"

setup
echo "original_content" > "$LOWER_DIR/cow_target.txt"
mount_fs

echo -n "Test 2.1: Write to lower-layer file triggers CoW... "
echo "modified" >> "$MOUNT_DIR/cow_target.txt" 2>/dev/null
if [ -f "$UPPER_DIR/cow_target.txt" ]; then
    pass "CoW copy created in upper"
else
    fail "no CoW copy in upper"
fi

echo -n "Test 2.2: Lower-layer file is unmodified after CoW... "
if grep -q "original_content" "$LOWER_DIR/cow_target.txt" 2>/dev/null && \
   ! grep -q "modified" "$LOWER_DIR/cow_target.txt" 2>/dev/null; then
    pass "lower unchanged"
else
    fail "lower was modified"
fi

echo -n "Test 2.3: Modified content is visible at mount point... "
if grep -q "modified" "$MOUNT_DIR/cow_target.txt" 2>/dev/null; then
    pass "modified content visible"
else
    fail "modified content not visible"
fi

echo -n "Test 2.4: New file created in upper layer... "
echo "brand_new" > "$MOUNT_DIR/new_file.txt" 2>/dev/null
if [ -f "$UPPER_DIR/new_file.txt" ] && \
   grep -q "brand_new" "$UPPER_DIR/new_file.txt" 2>/dev/null; then
    pass "new file written to upper"
else
    fail "new file not in upper"
fi

echo -n "Test 2.5: New file not present in lower layer... "
if [ ! -f "$LOWER_DIR/new_file.txt" ]; then
    pass "lower layer unchanged"
else
    fail "new file appeared in lower"
fi

teardown
echo ""

# SECTION 3: Whiteout Mechanism

echo "--- Section 3: Whiteout Mechanism ---"

setup
echo "to_be_deleted" > "$LOWER_DIR/delete_me.txt"
echo "also_delete"   > "$LOWER_DIR/delete_me2.txt"
mount_fs

echo -n "Test 3.1: Deleting lower-layer file creates whiteout... "
rm "$MOUNT_DIR/delete_me.txt" 2>/dev/null
if [ -f "$UPPER_DIR/.wh.delete_me.txt" ]; then
    pass "whiteout created"
else
    fail "no whiteout marker found"
fi

echo -n "Test 3.2: Deleted file no longer visible at mount point... "
if [ ! -f "$MOUNT_DIR/delete_me.txt" ]; then
    pass "file hidden by whiteout"
else
    fail "file still visible after delete"
fi

echo -n "Test 3.3: Lower layer file preserved (only whiteout in upper)... "
if [ -f "$LOWER_DIR/delete_me.txt" ]; then
    pass "lower unchanged"
else
    fail "lower was modified"
fi

echo -n "Test 3.4: Whiteout not visible in directory listing... "
if ! ls "$MOUNT_DIR" 2>/dev/null | grep -q "\.wh\."; then
    pass "whiteout files hidden"
else
    fail "whiteout file visible to user"
fi

echo -n "Test 3.5: Deleting upper-layer file removes it directly... "
echo "in_upper" > "$UPPER_DIR/upper_file.txt"
rm "$MOUNT_DIR/upper_file.txt" 2>/dev/null
if [ ! -f "$UPPER_DIR/upper_file.txt" ]; then
    pass "upper file removed directly"
else
    fail "upper file still present"
fi

teardown
echo ""

# SECTION 4: Directory Operations

echo "--- Section 4: Directory Operations ---"

setup
mkdir -p "$LOWER_DIR/subdir"
echo "sub_content" > "$LOWER_DIR/subdir/file.txt"
mount_fs

echo -n "Test 4.1: Subdirectory from lower is visible... "
if [ -d "$MOUNT_DIR/subdir" ]; then
    pass "subdir visible"
else
    fail "subdir not visible"
fi

echo -n "Test 4.2: Files within subdirectory are accessible... "
if grep -q "sub_content" "$MOUNT_DIR/subdir/file.txt" 2>/dev/null; then
    pass "subdir file readable"
else
    fail "subdir file not readable"
fi

echo -n "Test 4.3: Creating new directory appears in upper layer... "
mkdir "$MOUNT_DIR/newdir" 2>/dev/null
if [ -d "$UPPER_DIR/newdir" ]; then
    pass "new directory in upper"
else
    fail "new directory not in upper"
fi

echo -n "Test 4.4: Files from both layers listed without duplicates... "
echo "dup_content" > "$LOWER_DIR/dup.txt"
echo "dup_upper"   > "$UPPER_DIR/dup.txt"
DUP_COUNT=$(ls "$MOUNT_DIR" 2>/dev/null | grep -c "^dup.txt$" || true)
if [ "$DUP_COUNT" -eq 1 ]; then
    pass "no duplicates in listing"
else
    fail "duplicate entries found (count=$DUP_COUNT)"
fi

teardown
echo ""

# SECTION 5: Edge Cases

echo "--- Section 5: Edge Cases ---"

setup
mount_fs

echo -n "Test 5.1: Reading non-existent file returns error... "
if ! cat "$MOUNT_DIR/nonexistent.txt" 2>/dev/null; then
    pass "ENOENT returned for missing file"
else
    fail "missing file did not return error"
fi

echo -n "Test 5.2: Empty lower layer mounts cleanly... "
if [ -d "$MOUNT_DIR" ] && ls "$MOUNT_DIR" > /dev/null 2>&1; then
    pass "empty lower mounts OK"
else
    fail "empty lower mount failed"
fi

echo -n "Test 5.3: Writing to empty mount creates file in upper... "
echo "first_write" > "$MOUNT_DIR/created.txt" 2>/dev/null
if [ -f "$UPPER_DIR/created.txt" ]; then
    pass "created file in upper"
else
    fail "file not created in upper"
fi

echo -n "Test 5.4: Read-back of created file is correct... "
if grep -q "first_write" "$MOUNT_DIR/created.txt" 2>/dev/null; then
    pass "created file readable"
else
    fail "created file not readable"
fi

echo -n "Test 5.5: Overwriting an existing upper-layer file... "
echo "overwritten" > "$MOUNT_DIR/created.txt" 2>/dev/null
if grep -q "overwritten" "$MOUNT_DIR/created.txt" 2>/dev/null; then
    pass "overwrite succeeds"
else
    fail "overwrite did not take effect"
fi

echo -n "Test 5.6: Whiteout file does not appear in ls output... "
echo "hidden" > "$LOWER_DIR/tobehidden.txt"
touch "$UPPER_DIR/.wh.tobehidden.txt"
if ! ls "$MOUNT_DIR" 2>/dev/null | grep -q "tobehidden.txt"; then
    pass "pre-existing whiteout hides file"
else
    fail "file not hidden by pre-existing whiteout"
fi

teardown
echo ""

# SECTION 6: Persistence After Remount

echo "--- Section 6: Persistence After Remount ---"

setup
echo "persist_lower" > "$LOWER_DIR/persist.txt"
mount_fs

echo "persist_upper" >> "$MOUNT_DIR/persist.txt" 2>/dev/null
fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null || true
sleep 1

# Remount
"$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
sleep 1

echo -n "Test 6.1: CoW copy persists after remount... "
if [ -f "$UPPER_DIR/persist.txt" ] && \
   grep -q "persist_upper" "$MOUNT_DIR/persist.txt" 2>/dev/null; then
    pass "CoW data persisted"
else
    fail "CoW data lost after remount"
fi

echo -n "Test 6.2: Whiteout persists after remount... "
echo "del_persist" > "$LOWER_DIR/del_persist.txt"
fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null || true
sleep 1
"$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
sleep 1
rm "$MOUNT_DIR/del_persist.txt" 2>/dev/null
fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null || true
sleep 1
"$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"
sleep 1
if [ ! -f "$MOUNT_DIR/del_persist.txt" ]; then
    pass "whiteout persisted after remount"
else
    fail "deleted file reappeared after remount"
fi

teardown
echo ""

# Summary

echo "=================================================="
echo " Test Results Summary"
echo "=================================================="
echo -e "  ${GREEN}PASSED${NC}:  $PASS_COUNT"
echo -e "  ${RED}FAILED${NC}:  $FAIL_COUNT"
echo -e "  ${YELLOW}SKIPPED${NC}: $SKIP_COUNT"
echo "=================================================="

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
