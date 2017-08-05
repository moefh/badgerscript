/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stdlib.h>
#include <stdint.h>

struct fh_input;

struct fh_input_funcs {
  ssize_t (*read)(struct fh_input *in, char *line, ssize_t max_len);
  int (*close)(struct fh_input *in);
};

struct fh_src_loc {
  uint16_t line;
  uint16_t col;
};

struct fh_input *fh_open_input_file(const char *filename);
struct fh_input *fh_new_input(void *user_data, struct fh_input_funcs *funcs);
void *fh_get_input_user_data(struct fh_input *in);
int fh_close_input(struct fh_input *in);
ssize_t fh_input_read(struct fh_input *in, char *line, ssize_t max_len);

#endif /* FH_H_FILE */
