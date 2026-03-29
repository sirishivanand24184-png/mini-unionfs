Mini-UnionFS (Member 1)

Implemented Features:
- Path resolution (upper layer overrides lower)
- Whiteout mechanism (.wh files hide lower files)
- File operations: getattr, open, read
- Directory merging (readdir)
- Deletion using unlink with whiteout

Build:
make

Run:
./mini_unionfs <lower_dir> <upper_dir> <mountpoint>

Example:
./mini_unionfs lower upper /tmp/mnt
