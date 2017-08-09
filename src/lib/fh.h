/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <sys/types.h>
#include <stdint.h>
#include <stdarg.h>

struct fh_input;

struct fh_input_funcs {
  int (*read)(struct fh_input *in, char *line, int max_len);
  int (*close)(struct fh_input *in);
};

enum fh_value_type {
  // non-object values (no heap usage):
  FH_VAL_NUMBER,
  FH_VAL_C_FUNC,
  
#define FH_FIRST_OBJECT_VAL FH_VAL_STRING
  // objects (use heap):
  FH_VAL_STRING,
  FH_VAL_FUNC,
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
int fh_run_function(struct fh_program *prog, const char *func_name);
int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret);

const char *fh_get_error(struct fh_program *prog);
int fh_set_error(struct fh_program *prog, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap);

void fh_make_number(struct fh_program *prog, struct fh_value *val, double num);
int fh_make_string(struct fh_program *prog, struct fh_value *val, const char *str);
int fh_make_string_n(struct fh_program *prog, struct fh_value *val, const char *str, size_t str_len);
void fh_make_c_func(struct fh_program *prog, struct fh_value *val, fh_c_func func);

const char *fh_get_string(const struct fh_value *val);

#endif /* FH_H_FILE */
