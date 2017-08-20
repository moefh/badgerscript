/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined(__GNUC__)
#define FH_PRINTF_FORMAT(x,y) __attribute__((format (printf, (x), (y))))
#else
#define FH_PRINTF_FORMAT(x,y)
#endif

struct fh_input;
struct fh_program;
struct fh_value;

struct fh_input_funcs {
  struct fh_input *(*open)(struct fh_input *in, const char *filename);
  int (*read)(struct fh_input *in, char *line, int max_len);
  int (*close)(struct fh_input *in);
};

enum fh_value_type {
  // non-object values (completely contained inside struct fh_value)
  FH_VAL_NULL,
  FH_VAL_BOOL,
  FH_VAL_NUMBER,
  FH_VAL_C_FUNC,
  
#define FH_FIRST_OBJECT_VAL FH_VAL_STRING
  // objects
  FH_VAL_STRING,
  FH_VAL_ARRAY,
  FH_VAL_MAP,
  FH_VAL_UPVAL,
  FH_VAL_CLOSURE,
  FH_VAL_FUNC_DEF,
};

typedef int (*fh_c_func)(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

struct fh_named_c_func {
  const char *name;
  fh_c_func func;
};

struct fh_value {
  union {
    void *obj;
    fh_c_func c_func;
    double num;
    bool b;
  } data;
  enum fh_value_type type;
};

struct fh_input *fh_open_input_file(const char *filename);
struct fh_input *fh_open_input_string(const char *string);
struct fh_input *fh_new_input(const char *filename, void *user_data, struct fh_input_funcs *funcs);
void *fh_get_input_user_data(struct fh_input *in);
const char *fh_get_input_filename(struct fh_input *in);
struct fh_input *fh_open_input(struct fh_input *in, const char *filename);
int fh_close_input(struct fh_input *in);
int fh_read_input(struct fh_input *in, char *line, int max_len);

struct fh_program *fh_new_program(void);
void fh_free_program(struct fh_program *prog);
void fh_set_gc_frequency(struct fh_program *prog, int frequency);
int fh_add_c_func(struct fh_program *prog, const char *name, fh_c_func func);
int fh_add_c_funcs(struct fh_program *prog, const struct fh_named_c_func *funcs, int n_funcs);
int fh_compile_input(struct fh_program *prog, struct fh_input *in);
int fh_compile_file(struct fh_program *prog, const char *filename);
void fh_dump_bytecode(struct fh_program *prog);
int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret);

const char *fh_get_error(struct fh_program *prog);
int fh_set_error(struct fh_program *prog, const char *fmt, ...) FH_PRINTF_FORMAT(2,3);
int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap);
void fh_collect_garbage(struct fh_program *prog);

bool fh_val_is_true(struct fh_value *val);
bool fh_vals_are_equal(struct fh_value *v1, struct fh_value *v2);

#define fh_is_null(v)     ((v)->type == FH_VAL_NULL)
#define fh_is_bool(v)     ((v)->type == FH_VAL_BOOL)
#define fh_is_number(v)   ((v)->type == FH_VAL_NUMBER)
#define fh_is_c_func(v)   ((v)->type == FH_VAL_C_FUNC)
#define fh_is_string(v)   ((v)->type == FH_VAL_STRING)
#define fh_is_closure(v)  ((v)->type == FH_VAL_CLOSURE)
#define fh_is_array(v)    ((v)->type == FH_VAL_ARRAY)
#define fh_is_map(v)      ((v)->type == FH_VAL_MAP)

#define fh_new_null()     ((struct fh_value) { .type = FH_VAL_NULL })

#define fh_new_bool(bv)   ((struct fh_value) { .type = FH_VAL_BOOL, .data = { .b = !!(bv) }})
#define fh_get_bool(v)    ((v)->data.b)

#define fh_new_c_func(f)  ((struct fh_value) { .type = FH_VAL_C_FUNC, .data = { .c_func = (f) }})
#define fh_get_c_func(v)  ((v)->data.c_func)

#define fh_new_number(n)  ((struct fh_value) { .type = FH_VAL_NUMBER, .data = { .num = (n) }})
#define fh_get_number(v)  ((v)->data.num)

struct fh_value fh_new_string(struct fh_program *prog, const char *str);
struct fh_value fh_new_string_n(struct fh_program *prog, const char *str, size_t str_len);
const char *fh_get_string(const struct fh_value *str);

struct fh_value fh_new_array(struct fh_program *prog);
int fh_get_array_len(const struct fh_value *arr);
struct fh_value *fh_get_array_item(struct fh_value *arr, int index);
struct fh_value *fh_grow_array(struct fh_program *prog, struct fh_value *val, int num_items);

struct fh_value fh_new_map(struct fh_program *prog);
int fh_alloc_map_len(struct fh_value *map, uint32_t len);
int fh_next_map_key(struct fh_value *map, struct fh_value *key, struct fh_value *next_key);
int fh_get_map_value(struct fh_value *map, struct fh_value *key, struct fh_value *val);
int fh_add_map_entry(struct fh_program *prog, struct fh_value *map, struct fh_value *key, struct fh_value *val);
int fh_delete_map_entry(struct fh_value *map, struct fh_value *key);

#endif /* FH_H_FILE */
