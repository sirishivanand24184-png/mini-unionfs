#ifndef WHITEOUT_H
#define WHITEOUT_H

#define FUSE_USE_VERSION 31

#define WH_PREFIX     ".wh."
#define WH_PREFIX_LEN 4

/*
 * is_whiteout_file - Detect whether a bare filename is a whiteout marker.
 *
 * @name  bare filename (no directory component), e.g. ".wh.config.txt"
 *
 * Returns 1 if the name starts with ".wh.", 0 otherwise.
 */
int is_whiteout_file(const char *name);

/*
 * make_whiteout_name - Build a whiteout filename from the original filename.
 *
 * @orig  bare filename to be hidden, e.g. "config.txt"
 * @buf   output buffer; receives ".wh.config.txt"
 *
 * The caller must ensure buf is large enough (strlen(orig) + WH_PREFIX_LEN + 1).
 */
void make_whiteout_name(const char *orig, char *buf);

/*
 * strip_whiteout_prefix - Extract the original filename from a whiteout name.
 *
 * @whname  whiteout filename, e.g. ".wh.config.txt"
 * @buf     output buffer; receives "config.txt"
 *
 * Behaviour is undefined if whname does not start with ".wh.".
 */
void strip_whiteout_prefix(const char *whname, char *buf);

/*
 * is_whiteout_active - Check whether a FUSE path is hidden by a whiteout marker.
 *
 * @path       absolute FUSE path, e.g. "/subdir/config.txt"
 * @upper_dir  real filesystem path of the upper (writable) layer
 *
 * Returns 1 if the corresponding .wh.* marker exists in upper_dir, 0 otherwise.
 */
int is_whiteout_active(const char *path, const char *upper_dir);

/*
 * create_whiteout - Create a whiteout marker in the upper layer.
 *
 * @path       absolute FUSE path of the file to hide, e.g. "/config.txt"
 * @upper_dir  real filesystem path of the upper (writable) layer
 *
 * Returns 0 on success, -errno on failure.
 */
int create_whiteout(const char *path, const char *upper_dir);

#endif /* WHITEOUT_H */
