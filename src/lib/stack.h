/* stack.h */

#ifndef STACK_H_FILE
#define STACK_H_FILE

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <stdio.h>

struct fh_stack {
  void *data;
  int num;
  int cap;
};

void fh_init_stack(struct fh_stack *s);
void fh_free_stack(struct fh_stack *s);
int fh_stack_shrink_to_fit(struct fh_stack *s, size_t item_size);
int fh_copy_stack(struct fh_stack *dst, const struct fh_stack *src, size_t item_size);
int fh_stack_size(struct fh_stack *s);
void *fh_push(struct fh_stack *s, void *item, size_t item_size);
int fh_pop(struct fh_stack *s, void *item, size_t item_size);
void *fh_stack_item(struct fh_stack *s, int index, size_t item_size);
void *fh_stack_top(struct fh_stack *s, size_t item_size);
void *fh_stack_next(struct fh_stack *s, void *cur, size_t item_size);

int fh_stack_ensure_cap(struct fh_stack *s, int n_items, size_t item_size);

#if 0
#define DECLARE_STACK(name, type)                                       \
  struct name { struct fh_stack s; };                                   \
  static inline void name##_init(struct name *n) {                      \
    fh_init_stack(&n->s);                                               \
  }                                                                     \
  static inline void name##_free(struct name *n) {                      \
    fh_free_stack(&n->s);                                               \
  }                                                                     \
  static inline int name##_shrink_to_fit(struct name *n) {              \
    return fh_stack_shrink_to_fit(&n->s, sizeof(type));                 \
  }                                                                     \
  static inline int name##_copy(struct name *dst, struct name *src) {   \
    return fh_copy_stack(&dst->s, &src->s, sizeof(type));               \
  }                                                                     \
  static inline int name##_size(struct name *n) {                       \
    return n->s.num;                                                    \
  }                                                                     \
  static inline void name##_set_size(struct name *n, int size) {        \
    n->s.num = size;                                                    \
  }                                                                     \
  static inline type *name##_data(struct name *n) {                     \
    return n->s.data;                                                   \
  }                                                                     \
  static inline type *name##_item(struct name *n, int index) {          \
    return fh_stack_item(&n->s, index, sizeof(type));                   \
  }                                                                     \
  static inline type *name##_top(struct name *n) {                      \
    return fh_stack_top(&n->s, sizeof(type));                           \
  }                                                                     \
  static inline type *name##_next(struct name *n, type *item) {         \
    return fh_stack_next(&n->s, item, sizeof(type));                    \
  }                                                                     \
  static inline int name##_pop(struct name *n, type *item) {            \
    return fh_pop(&n->s, item, sizeof(type));                           \
  }                                                                     \
  static inline type *name##_push(struct name *n, type *item) {         \
    return fh_push(&n->s, item, sizeof(type));                          \
  }                                                                     \
  struct name  /* keep some compilers happy about extra ';' */
#else
/*
 * Inlining push, pop, item, top makes a considerable difference for
 * function calls (30% faster on tests/bench_call.fh).  It increases
 * the code size a bit, though.
 */
#define DECLARE_STACK(name, type)                                       \
  struct name { struct fh_stack s; };                                   \
  static inline void name##_init(struct name *n) {                      \
    fh_init_stack(&n->s);                                               \
  }                                                                     \
  static inline void name##_free(struct name *n) {                      \
    fh_free_stack(&n->s);                                               \
  }                                                                     \
  static inline int name##_size(struct name *n) {                       \
    return n->s.num;                                                    \
  }                                                                     \
  static inline type *name##_data(struct name *n) {                     \
    return n->s.data;                                                   \
  }                                                                     \
  static inline int name##_shrink_to_fit(struct name *n) {              \
    return fh_stack_shrink_to_fit(&n->s, sizeof(type));                 \
  }                                                                     \
  static inline int name##_copy(struct name *dst, struct name *src) {   \
    return fh_copy_stack(&dst->s, &src->s, sizeof(type));               \
  }                                                                     \
  static inline void name##_set_size(struct name *n, int size) {        \
    n->s.num = size;                                                    \
  }                                                                     \
  static inline type *name##_item(struct name *n, int index) {          \
    if (index < 0 || index >= n->s.num) return NULL;                    \
    return (type*)n->s.data + index;                                    \
  }                                                                     \
  static inline type *name##_top(struct name *n) {                      \
    if (n->s.num == 0) return NULL;                                     \
    return (type*) n->s.data + (n->s.num-1);                            \
  }                                                                     \
  static inline type *name##_next(struct name *n, type *item) {         \
    return fh_stack_next(&n->s, item, sizeof(type));                    \
  }                                                                     \
  static inline int name##_pop(struct name *n, type *item) {            \
    if (n->s.num == 0) return -1;                                       \
    n->s.num--;                                                         \
    if (item) *item = ((type*)n->s.data)[n->s.num];                     \
    return 0;                                                           \
  }                                                                     \
  static inline type *name##_push(struct name *n, type *item) {         \
    if (n->s.num + 1 > n->s.cap &&                                      \
        fh_stack_ensure_cap(&n->s, 1, sizeof(type)) < 0)                \
      return NULL;                                                      \
    type *dest = (type*)n->s.data + n->s.num;                           \
    if (item) *dest = *item; else memset(dest, 0, sizeof(type));        \
    n->s.num++;                                                         \
    return dest;                                                        \
  }                                                                     \
  struct name  /* keep some compilers happy about extra ';' */
#endif

#define stack_foreach(type, star, v, st)                             \
  for (type *v = NULL;                                               \
       (v = (type *)fh_stack_next(&(st)->s,v,sizeof(type))) != NULL; \
       )

#endif /* STACK_H_FILE */
