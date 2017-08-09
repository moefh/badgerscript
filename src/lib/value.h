/* value.h */

#ifndef VALUE_H_FILE
#define VALUE_H_FILE

#include "fh.h"

#define OBJ_HEADER  \
  struct fh_object *next; \
  enum fh_value_type type

struct fh_string {
  OBJ_HEADER;
  size_t size;
};

struct fh_func {
  OBJ_HEADER;
  int n_params;
  int n_regs;
  uint32_t *code;
  int code_size;
  struct fh_value *consts;
  int n_consts;
};

struct fh_object {
  union {
    double align;
    struct {
      OBJ_HEADER;
    } header;
    struct fh_string str;
    struct fh_func func;
  } obj;
};

struct fh_object *fh_new_object(struct fh_program *prog, enum fh_value_type type, size_t extra_size);
void fh_free_object(struct fh_object *obj);

struct fh_func *fh_get_func(const struct fh_value *val);

#define VAL_IS_OBJECT(v)  ((v)->type >= FH_FIRST_OBJECT_VAL)
#define GET_OBJ_STRING(o) (((char *) o) + sizeof(struct fh_object))
#define GET_OBJ_FUNC(o)   ((struct fh_func *) (o))

#endif /* VALUE_H_FILE */
