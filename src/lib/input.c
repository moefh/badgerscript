/* input.c */

#include <stdlib.h>
#include <stdio.h>

#include "fh.h"

struct fh_input {
  void *user_data;
  struct fh_input_funcs *funcs;
};

struct fh_input *fh_new_input(void *user_data, struct fh_input_funcs *funcs)
{
  struct fh_input *in = malloc(sizeof(struct fh_input));
  if (! in)
    return NULL;
  in->user_data = user_data;
  in->funcs = funcs;
  return in;
}

void *fh_get_input_user_data(struct fh_input *in)
{
  return in->user_data;
}

int fh_close_input(struct fh_input *in)
{
  int ret = in->funcs->close(in);
  free(in);
  return ret;
}

ssize_t fh_input_read(struct fh_input *in, uint8_t *line, ssize_t max_len)
{
  return in->funcs->read(in, line, max_len);
}

/* ======================================= */
/* === file input ======================== */

static ssize_t file_read(struct fh_input *in, uint8_t *line, ssize_t max_len)
{
  FILE *f = in->user_data;

  size_t ret = fread(line, 1, max_len, f);
  if (ret == 0) {
    if (ferror(f) || feof(f))
      return -1;
  }
  return ret;
}

static int file_close(struct fh_input *in)
{
  FILE *f = in->user_data;

  return fclose(f);
}

struct fh_input *fh_open_input_file(const char *filename)
{
  static struct fh_input_funcs file_input_funcs = {
    .read = file_read,
    .close = file_close,
  };

  FILE *f = fopen(filename, "rb");
  if (! f)
    return NULL;

  struct fh_input *in = fh_new_input(f, &file_input_funcs);
  if (! in) {
    fclose(f);
    return NULL;
  }
  return in;
}
