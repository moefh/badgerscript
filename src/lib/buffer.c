/* buffer.c */

#include <limits.h>
#include <string.h>

#include "fh_i.h"

void fh_init_buffer(struct fh_buffer *buf)
{
  buf->p = NULL;
  buf->size = 0;
  buf->cap = 0;
}

void fh_free_buffer(struct fh_buffer *buf)
{
  if (buf->p != NULL)
    free(buf->p);
  buf->p = NULL;
  buf->size = 0;
  buf->cap = 0;
}

int fh_buf_grow(struct fh_buffer *buf, size_t add_size)
{
  size_t new_size = buf->size + add_size;
  if (new_size > INT_MAX || new_size < (size_t)buf->size)
    return -1;
  if ((int)new_size > buf->cap) {
    size_t new_cap = ((new_size + 1024 + 1) / 1024) * 1024;
    if (new_cap > INT_MAX || new_cap < new_size)
      return -1;
    void *new_p = realloc(buf->p, new_cap);
    if (new_p == NULL)
      return -1;
    buf->p = new_p;
    buf->cap = (int) new_cap;
  }
  
  buf->size = (int) new_size;
  return 0;
}

int fh_buf_add_string(struct fh_buffer *buf, const char *str, size_t str_size)
{
  int pos = buf->size;
  if (fh_buf_grow(buf, str_size + 1) < 0)
    return -1;
  memcpy(buf->p + pos, str, str_size);
  buf->p[pos + str_size] = '\0';
  return pos;
}

int fh_buf_add_byte(struct fh_buffer *buf, uint8_t c)
{
  int pos = buf->size;
  if (fh_buf_grow(buf, 1) < 0)
    return -1;
  buf->p[pos++] = c;
  return pos;
}
