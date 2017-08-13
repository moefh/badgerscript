/* value.h */

#ifndef VALUE_H_FILE
#define VALUE_H_FILE

#include "fh.h"
#include "stack.h"

DECLARE_STACK(value_stack, struct fh_value);

#define OBJ_HEADER  \
  struct fh_object *next;  \
  uint8_t gc_mark;         \
  enum fh_value_type type

struct fh_object_header {
  OBJ_HEADER;
};

struct fh_string {
  OBJ_HEADER;
  size_t size;
};

struct fh_array {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_value *items;
  int len;
  int cap;
};

struct fh_func_def {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_string *name;
  int n_params;
  int n_regs;
  uint32_t *code;
  int code_size;
  struct fh_value *consts;
  int n_consts;
};

struct fh_closure {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_func_def *func_def;
  /* TODO: environment */
};

struct fh_object {
  union {
    double align;
    struct fh_object_header header;
    struct fh_string str;
    struct fh_func_def func_def;
    struct fh_closure closure;
  } obj;
};

#define VAL_IS_OBJECT(v)  ((v)->type >= FH_FIRST_OBJECT_VAL)

#define GET_OBJ_CLOSURE(o)     ((struct fh_closure *) (o))
#define GET_OBJ_FUNC_DEF(o)    ((struct fh_func_def *) (o))
#define GET_OBJ_ARRAY(o)       ((struct fh_array *) (o))
#define GET_OBJ_STRING(o)      ((struct fh_string *) (o))
#define GET_OBJ_STRING_DATA(o) (((char *) o) + sizeof(struct fh_string))

#define GET_VAL_CLOSURE(v)   (((v)->type == FH_VAL_CLOSURE ) ? ((struct fh_closure  *) ((v)->data.obj)) : NULL)
#define GET_VAL_FUNC_DEF(v)  (((v)->type == FH_VAL_FUNC_DEF) ? ((struct fh_func_def *) ((v)->data.obj)) : NULL)
#define GET_VAL_ARRAY(v)     (((v)->type == FH_VAL_ARRAY   ) ? ((struct fh_array    *) ((v)->data.obj)) : NULL)

// non-object types
#define fh_make_null   fh_new_null
#define fh_make_number fh_new_number
#define fh_make_c_func fh_new_c_func

// object types
struct fh_func_def *fh_make_func_def(struct fh_program *prog);
struct fh_closure *fh_make_closure(struct fh_program *prog);
struct fh_array *fh_make_array(struct fh_program *prog);
struct fh_string *fh_make_string(struct fh_program *prog, const char *str);
struct fh_string *fh_make_string_n(struct fh_program *prog, const char *str, size_t str_len);

// object functions
void fh_free_object(struct fh_object *obj);
struct fh_value *fh_grow_array_object(struct fh_program *prog, struct fh_array *arr, int num_items);
const char *fh_get_func_def_name(struct fh_func_def *func_def);

#endif /* VALUE_H_FILE */
