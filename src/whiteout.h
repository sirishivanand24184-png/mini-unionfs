#ifndef WHITEOUT_H
#define WHITEOUT_H

#define WHITEOUT_PREFIX     ".wh."
#define WHITEOUT_PREFIX_LEN 4

int is_whiteout_file(const char *filename);
void make_whiteout_name(const char *filename, char *out_buf);
void strip_whiteout_prefix(const char *whiteout_name, char *out_buf);
int is_whiteout_active(const char *path, const char *upper_dir);
int create_whiteout(const char *path, const char *upper_dir);

#endif
