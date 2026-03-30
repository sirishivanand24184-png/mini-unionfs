CC        = gcc
PREFIX    = /usr/local
BINDIR    = $(PREFIX)/bin

# Detect FUSE3 library via pkg-config
FUSE_CFLAGS := $(shell pkg-config --cflags fuse3 2>/dev/null)
FUSE_LIBS   := $(shell pkg-config --libs   fuse3 2>/dev/null)

ifeq ($(FUSE_CFLAGS),)
  $(error "libfuse3-dev not found. Install with: sudo apt install libfuse3-dev")
endif

# Common flags
COMMON_CFLAGS = -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes \
                -Wold-style-definition $(FUSE_CFLAGS)

# Release flags (default)
RELEASE_CFLAGS = $(COMMON_CFLAGS) -O2 -DNDEBUG

# Debug flags
DEBUG_CFLAGS = $(COMMON_CFLAGS) -g -O0 -DDEBUG -fsanitize=address

CFLAGS ?= $(RELEASE_CFLAGS)

SRC = src/main.c \
      src/path_resolution.c \
      src/fuse_ops_core.c \
      src/file_operations.c \
      src/directory_ops.c \
      src/whiteout.c

OBJ = $(SRC:.c=.o)

OUT = mini_unionfs

# C unit test binaries (no FUSE dependency needed at link)
TEST_COW      = tests/test_cow
TEST_WHITEOUT = tests/test_whiteout

# Generate dependency files alongside object files
DEPFLAGS = -MMD -MP
DEPS     = $(OBJ:.o=.d)

.PHONY: all release debug install uninstall clean test test-unit test-shell

# ---------- default target ----------
all: release

release: CFLAGS = $(RELEASE_CFLAGS)
release: $(OUT)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(OUT)

# ---------- link main binary ----------
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) $^ $(FUSE_LIBS) -o $@

# ---------- compile main objects ----------
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)

# ---------- build C unit tests ----------
$(TEST_COW): tests/test_cow.c
	$(CC) -Wall -Wextra -o $@ $<

$(TEST_WHITEOUT): tests/test_whiteout.c src/whiteout.c
	$(CC) -Wall -Wextra -I src $(FUSE_CFLAGS) -o $@ $^

# ---------- install / uninstall ----------
install: release
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(OUT) $(DESTDIR)$(BINDIR)/$(OUT)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(OUT)

# ---------- test targets ----------
test-unit: $(TEST_COW) $(TEST_WHITEOUT)
	@echo "Running C unit tests..."
	@$(TEST_COW)
	@$(TEST_WHITEOUT)

test-shell: release
	@echo "Running shell test suite..."
	@bash tests/test_unionfs.sh

test: test-unit test-shell

# ---------- clean ----------
clean:
	rm -f $(OBJ) $(DEPS) $(OUT) $(TEST_COW) $(TEST_WHITEOUT)
