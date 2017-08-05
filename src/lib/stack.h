/* stack.h */

#ifndef STACK_H_FILE
#define STACK_H_FILE

#include <stdlib.h>
#include <stdint.h>

struct fh_stack {
  void *data;
  size_t item_size;
  int num;
  int cap;
};

void fh_init_stack(struct fh_stack *s, size_t item_size);
void fh_free_stack(struct fh_stack *s);
int fh_stack_ensure_size(struct fh_stack *s, int n_items);
int fh_stack_grow(struct fh_stack *s, int n_items);
int fh_stack_shrink_to_fit(struct fh_stack *s);
int fh_stack_is_empty(struct fh_stack *s);
int fh_stack_count(struct fh_stack *s);
void *fh_push(struct fh_stack *s, void *item);
int fh_pop(struct fh_stack *s, void *item);
void *fh_stack_item(struct fh_stack *s, int index);
void *fh_stack_top(struct fh_stack *s);
void *fh_stack_next(struct fh_stack *s, void *cur);

#define stack_foreach(type, v, s) for (type v = NULL; (v = fh_stack_next(s, v)) != NULL; )

#endif /* STACK_H_FILE */
