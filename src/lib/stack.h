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
int fh_stack_shrink_to_fit(struct fh_stack *s);
int fh_copy_stack(struct fh_stack *dst, const struct fh_stack *src);
int fh_stack_size(struct fh_stack *s);
void *fh_push(struct fh_stack *s, void *item);
int fh_pop(struct fh_stack *s, void *item);
void *fh_stack_item(struct fh_stack *s, int index);
void *fh_stack_top(struct fh_stack *s);
void *fh_stack_next(struct fh_stack *s, void *cur);

#define stack_foreach(type, v, s) for (type v = NULL; (v = fh_stack_next(s, v)) != NULL; )

#define DECLARE_STACK(name, type)                                                                    \
  typedef struct { struct fh_stack s; } name;                                                        \
  static inline void  name##_init(name *n) { fh_init_stack(&n->s, sizeof(type)); }                   \
  static inline void  name##_free(name *n) { fh_free_stack(&n->s); }                                 \
  static inline int   name##_shrink_to_fit(name *n) { return fh_stack_shrink_to_fit(&n->s); }        \
  static inline int   name##_copy(name *dst, name *src) { return fh_copy_stack(&dst->n, &src->n); }  \
  static inline int   name##_pop(name *n, type *item) { return fh_pop(&n->s, item); }                \
  static inline type *name##_push(name *n, type *item) { return fh_push(&n->s, item); }              \
  static inline type *name##_next(name *n, type *item) { return fh_stack_next(&n->s, item); }

#endif /* STACK_H_FILE */
