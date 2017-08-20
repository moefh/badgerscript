/* value.h */

#ifndef VALUE_H_FILE
#define VALUE_H_FILE

#include "fh_internal.h"
#include "stack.h"

enum fh_upval_def_type {
  FH_UPVAL_TYPE_REG,
  FH_UPVAL_TYPE_UPVAL,
};

DECLARE_STACK(value_stack, struct fh_value);

#define GC_BIT_MARK  (1<<0)
#define GC_BIT_PIN   (1<<1)

#define GC_SET_BIT(o,b)   (((struct fh_object *)o)->obj.header.gc_bits|=(b))
#define GC_CLEAR_BIT(o,b) (((struct fh_object *)o)->obj.header.gc_bits&=~(b))
#define GC_PIN_OBJ(o)        GC_SET_BIT(o, GC_BIT_PIN)
#define GC_UNPIN_OBJ(o)      GC_CLEAR_BIT(o, GC_BIT_PIN)

#define OBJ_HEADER  \
  struct fh_object *next;  \
  uint8_t gc_bits;         \
  enum fh_value_type type

struct fh_object_header {
  OBJ_HEADER;
};

struct fh_string {
  OBJ_HEADER;
  uint32_t size;
  uint32_t hash;
};

struct fh_array {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_value *items;
  int len;
  int cap;
};

struct fh_map_entry {
  struct fh_value key;
  struct fh_value val;
};

struct fh_map {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_map_entry *entries;
  uint32_t len;
  uint32_t cap;
};

struct fh_upval_def {
  enum fh_upval_def_type type;
  int num;
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
  struct fh_upval_def *upvals;
  int n_upvals;
  int code_src_loc_size;
  void *code_src_loc;
  fh_symbol_id src_file_id;
};

struct fh_upval {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_value *val;
  union {
    struct fh_value storage;
    struct fh_upval *next;
  } data;
};

struct fh_closure {
  OBJ_HEADER;
  struct fh_object *gc_next_container;
  struct fh_func_def *func_def;
  int n_upvals;
  struct fh_upval **upvals;
};

struct fh_object {
  union {
    double align;
    struct fh_object_header header;
    struct fh_string str;
    struct fh_func_def func_def;
    struct fh_upval upval;
    struct fh_closure closure;
    struct fh_array array;
    struct fh_map map;
  } obj;
};

#define VAL_IS_OBJECT(v)  ((v)->type >= FH_FIRST_OBJECT_VAL)

#define GET_OBJ_CLOSURE(o)     ((struct fh_closure   *) (o))
#define GET_OBJ_UPVAL(o)       ((struct fh_upval     *) (o))
#define GET_OBJ_FUNC_DEF(o)    ((struct fh_func_def  *) (o))
#define GET_OBJ_ARRAY(o)       ((struct fh_array     *) (o))
#define GET_OBJ_MAP(o)         ((struct fh_map       *) (o))
#define GET_OBJ_STRING(o)      ((struct fh_string    *) (o))
#define GET_OBJ_STRING_DATA(o) (((char *) (o)) + sizeof(struct fh_string))

#define GET_VAL_OBJ(v)         ((struct fh_object *) ((v)->data.obj))
#define GET_VAL_CLOSURE(v)     (((v)->type == FH_VAL_CLOSURE ) ? ((struct fh_closure  *) ((v)->data.obj)) : NULL)
#define GET_VAL_FUNC_DEF(v)    (((v)->type == FH_VAL_FUNC_DEF) ? ((struct fh_func_def *) ((v)->data.obj)) : NULL)
#define GET_VAL_ARRAY(v)       (((v)->type == FH_VAL_ARRAY   ) ? ((struct fh_array    *) ((v)->data.obj)) : NULL)
#define GET_VAL_MAP(v)         (((v)->type == FH_VAL_MAP     ) ? ((struct fh_map      *) ((v)->data.obj)) : NULL)
#define GET_VAL_STRING(v)      (((v)->type == FH_VAL_STRING  ) ? ((struct fh_string   *) ((v)->data.obj)) : NULL)
#define GET_VAL_STRING_DATA(v) (((v)->type == FH_VAL_STRING  ) ? ((const char *) ((v)->data.obj) + sizeof(struct fh_string)) : NULL)

#define UPVAL_IS_OPEN(uv)    ((uv)->val != (uv)->data.storage)

// non-object types
#define fh_make_null   fh_new_null
#define fh_make_bool   fh_new_bool
#define fh_make_number fh_new_number
#define fh_make_c_func fh_new_c_func

// object types
struct fh_func_def *fh_make_func_def(struct fh_program *prog, bool pinned);
struct fh_closure *fh_make_closure(struct fh_program *prog, bool pinned);
struct fh_upval *fh_make_upval(struct fh_program *prog, bool pinned);
struct fh_array *fh_make_array(struct fh_program *prog, bool pinned);
struct fh_map *fh_make_map(struct fh_program *prog, bool pinned);
struct fh_string *fh_make_string(struct fh_program *prog, bool pinned, const char *str);
struct fh_string *fh_make_string_n(struct fh_program *prog, bool pinned, const char *str, size_t str_len);

// object functions
void fh_free_object(struct fh_object *obj);
struct fh_value *fh_grow_array_object(struct fh_program *prog, struct fh_array *arr, int num_items);
const char *fh_get_func_def_name(struct fh_func_def *func_def);
int fh_alloc_map_object_len(struct fh_map *map, uint32_t len);
int fh_next_map_object_key(struct fh_map *map, struct fh_value *key, struct fh_value *next_key);
int fh_get_map_object_value(struct fh_map *map, struct fh_value *key, struct fh_value *val);
int fh_add_map_object_entry(struct fh_program *prog, struct fh_map *map, struct fh_value *key, struct fh_value *val);
int fh_delete_map_object_entry(struct fh_map *map, struct fh_value *key);

#endif /* VALUE_H_FILE */
