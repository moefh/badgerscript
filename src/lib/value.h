/* value.h */

#ifndef VALUE_H_FILE
#define VALUE_H_FILE

#include "fh.h"

#define OBJ_HEADER  \
  struct fh_object *next;  \
  uint8_t gc_mark;         \
  enum fh_value_type type

struct fh_string {
  OBJ_HEADER;
  size_t size;
};

struct fh_func {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
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

void fh_free_object(struct fh_object *obj);

struct fh_func *fh_get_func(const struct fh_value *val);

#define VAL_IS_OBJECT(v)  ((v)->type >= FH_FIRST_OBJECT_VAL)
#define GET_OBJ_STRING(o) (((char *) o) + sizeof(struct fh_object))
#define GET_OBJ_FUNC(o)   ((struct fh_func *) (o))

// non-object types
#define fh_make_null   fh_new_null
#define fh_make_number fh_new_number
#define fh_make_c_func fh_new_c_func

// object types
struct fh_func *fh_make_func(struct fh_program *prog);
struct fh_string *fh_make_string(struct fh_program *prog, const char *str);
struct fh_string *fh_make_string_n(struct fh_program *prog, const char *str, size_t str_len);

#endif /* VALUE_H_FILE */
