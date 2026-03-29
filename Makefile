CC = gcc
CFLAGS = -Wall `pkg-config fuse3 --cflags`
LIBS = `pkg-config fuse3 --libs`

SRC = src/main.c \
      src/path_resolution.c \
      src/fuse_ops_core.c \
      src/file_operations.c \
      src/directory_ops.c \
      src/whiteout.c

OUT = mini_unionfs

all:
	$(CC) $(SRC) $(CFLAGS) $(LIBS) -o $(OUT)

clean:
	rm -f $(OUT)
