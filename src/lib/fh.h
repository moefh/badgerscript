/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

struct fh_input;

struct fh_input_funcs {
  int (*read)(struct fh_input *in, char *line, int max_len);
  int (*close)(struct fh_input *in);
};

enum fh_value_type {
  // non-object values (completely contained inside struct fh_value)
  FH_VAL_NULL,
  FH_VAL_NUMBER,
  FH_VAL_C_FUNC,
  
#define FH_FIRST_OBJECT_VAL FH_VAL_STRING
  // objects
  FH_VAL_STRING,
  FH_VAL_ARRAY,
  FH_VAL_CLOSURE,
  FH_VAL_FUNC_DEF,
};

struct fh_program;
struct fh_value;
struct fh_object;

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
  } data;
  enum fh_value_type type;
};

struct fh_input *fh_open_input_file(const char *filename);
struct fh_input *fh_new_input(void *user_data, struct fh_input_funcs *funcs);
void *fh_get_input_user_data(struct fh_input *in);
int fh_close_input(struct fh_input *in);
int fh_input_read(struct fh_input *in, char *line, int max_len);

struct fh_program *fh_new_program(void);
void fh_free_program(struct fh_program *prog);
int fh_add_c_func(struct fh_program *prog, const char *name, fh_c_func func);
int fh_add_c_funcs(struct fh_program *prog, const struct fh_named_c_func *funcs, int n_funcs);
int fh_compile_file(struct fh_program *prog, const char *filename);
void fh_dump_bytecode(struct fh_program *prog);
int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret);

const char *fh_get_error(struct fh_program *prog);
int fh_set_error(struct fh_program *prog, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap);
void fh_collect_garbage(struct fh_program *prog);

// misc values
#define fh_new_null(prog) ((prog)->null_value)
struct fh_value fh_new_c_func(struct fh_program *prog, fh_c_func func);

// number
struct fh_value fh_new_number(struct fh_program *prog, double num);
#define fh_get_number(val) ((val)->data.num)

// string
struct fh_value fh_new_string(struct fh_program *prog, const char *str);
struct fh_value fh_new_string_n(struct fh_program *prog, const char *str, size_t str_len);
const char *fh_get_string(const struct fh_value *str);

// array
struct fh_value fh_new_array(struct fh_program *prog);
int fh_get_array_len(const struct fh_value *arr);
struct fh_value *fh_get_array_item(struct fh_value *arr, int index);
struct fh_value *fh_grow_array(struct fh_program *prog, struct fh_value *val, int num_items);

#endif /* FH_H_FILE */
