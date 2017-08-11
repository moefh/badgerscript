/* stack.c */

#include <stdlib.h>
#include <string.h>

#include "stack.h"

void fh_init_stack(struct fh_stack *s)
{
  s->data = NULL;
  s->num = 0;
  s->cap = 0;
}

void fh_free_stack(struct fh_stack *s)
{
  if (s->data) {
    free(s->data);
    s->data = NULL;
  }
  s->num = 0;
  s->cap = 0;
}

int fh_stack_size(struct fh_stack *s)
{
  return s->num;
}

void *fh_stack_top(struct fh_stack *s, size_t item_size)
{
  return fh_stack_item(s, s->num-1, item_size);
}

void *fh_stack_item(struct fh_stack *s, int index, size_t item_size)
{
  if (index < 0 || index >= s->num)
    return NULL;
  return (char *) s->data + index*item_size;
}

int fh_stack_shrink_to_fit(struct fh_stack *s, size_t item_size)
{
  s->cap = s->num;

  if (s->num == 0) {
    if (s->data)
      free(s->data);
    s->data = NULL;
  } else {
    void *new_data = realloc(s->data, s->num * item_size);
    if (new_data == NULL)
      return -1;
    s->data = new_data;
  }
  return 0;
}

int fh_stack_ensure_cap(struct fh_stack *s, int n_items, size_t item_size)
{
  if (s->num + n_items > s->cap) {
    int new_cap = (s->num + n_items + 15) / 16 * 16;
    void *new_data = realloc(s->data, new_cap * item_size);
    if (new_data == NULL)
      return -1;
    s->data = new_data;
    s->cap = new_cap;
  }
  return 0;
}

int fh_copy_stack(struct fh_stack *dst, const struct fh_stack *src, size_t item_size)
{
  if (dst->cap < src->num)
    if (fh_stack_ensure_cap(dst, src->num - dst->num, item_size) < 0)
      return -1;
  if (src->num > 0)
    memcpy(dst->data, src->data, item_size*src->num);
  dst->num = src->num;
  return 0;
}

void *fh_push(struct fh_stack *s, void *item, size_t item_size)
{
  if (fh_stack_ensure_cap(s, 1, item_size) < 0)
    return NULL;

  if (item)
    memcpy((char *) s->data + s->num*item_size, item, item_size);
  else
    memset((char *) s->data + s->num*item_size, 0, item_size);
  return (char *) s->data + (s->num++)*item_size;
}

int fh_pop(struct fh_stack *s, void *item, size_t item_size)
{
  if (s->num == 0)
    return -1;

  s->num--;
  if (item)
    memcpy(item, (char *) s->data + s->num*item_size, item_size);
  return 0;
}

void *fh_stack_next(struct fh_stack *s, void *cur, size_t item_size)
{
  if (cur == NULL)
    return (s->num > 0) ? s->data : NULL;
  cur = (char *) cur + item_size;
  if (cur >= (void *) ((char *) s->data + s->num*item_size))
    return NULL;
  return cur;
}
