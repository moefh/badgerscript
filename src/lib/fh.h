/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stdlib.h>
#include <stdint.h>

struct fh_input_funcs {
  ssize_t (*read)(void *src, uint8_t *line, ssize_t max_len);
};

struct fh_input {
  void *src;
  struct fh_input_funcs *funcs;
};

struct fh_src_loc {
  uint16_t line;
  uint16_t col;
};

enum fh_op_assoc {
  FH_ASSOC_PREFIX,
  FH_ASSOC_LEFT,
  FH_ASSOC_RIGHT,
};

void fh_error(const char *msg);
const char *fh_get_error(void);
int fh_input_open_file(struct fh_input *in, const char *filename);
int fh_input_close_file(struct fh_input *in);

#endif /* FH_H_FILE */
