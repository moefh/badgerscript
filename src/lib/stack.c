/* stack.c */

#include <stdlib.h>
#include <string.h>

#include "stack.h"

void fh_init_stack(struct fh_stack *s, size_t item_size)
{
  s->item_size = item_size;
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

int fh_stack_is_empty(struct fh_stack *s)
{
  return s->num == 0;
}

int fh_stack_count(struct fh_stack *s)
{
  return s->num;
}

void *fh_stack_top(struct fh_stack *s)
{
  return fh_stack_item(s, s->num-1);
}

void *fh_stack_item(struct fh_stack *s, int index)
{
  if (index >= s->num)
    return NULL;
  return (char *) s->data + index*s->item_size;
}

int fh_stack_shrink_to_fit(struct fh_stack *s)
{
  s->cap = s->num;

  if (s->num == 0) {
    if (s->data)
      free(s->data);
    s->data = NULL;
  } else {
    void *new_data = realloc(s->data, s->num * s->item_size);
    if (new_data == NULL)
      return -1;
    s->data = new_data;
  }
  return 0;
}

int fh_push(struct fh_stack *s, void *item)
{
  if (s->num >= s->cap) {
    int new_cap = (s->cap + 16 + 1) / 16 * 16;
    void *new_data = realloc(s->data, new_cap * s->item_size);
    if (new_data == NULL)
      return -1;
    s->data = new_data;
    s->cap = new_cap;
  }

  if (item)
    memcpy((char *) s->data + s->num*s->item_size, item, s->item_size);
  else
    memset((char *) s->data + s->num*s->item_size, 0, s->item_size);
  s->num++;
  return 0;
}

int fh_pop(struct fh_stack *s, void *item)
{
  if (s->num == 0)
    return -1;

  s->num--;
  if (item)
    memcpy(item, (char *) s->data + s->num*s->item_size, s->item_size);
  return 0;
}

void *fh_stack_next(struct fh_stack *s, void *cur)
{
  if (cur == NULL)
    return s->data;
  cur = (char *) cur + s->item_size;
  if (cur >= (void *) ((char *) s->data + s->num*s->item_size))
    return NULL;
  return cur;
}
