/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

struct fh_input;
struct fh_program;

struct fh_input_funcs {
  ssize_t (*read)(struct fh_input *in, char *line, ssize_t max_len);
  int (*close)(struct fh_input *in);
};

enum fh_value_type {
  FH_VAL_NUMBER,
  FH_VAL_STRING,
  FH_VAL_FUNC,
  FH_VAL_C_FUNC,
};

struct fh_value;

typedef int (*fh_c_func)(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

struct fh_value {
  union {
    double num;
    char *str;
    struct fh_bc_func *func;
    fh_c_func c_func;
  } data;
  enum fh_value_type type;
};

struct fh_input *fh_open_input_file(const char *filename);
struct fh_input *fh_new_input(void *user_data, struct fh_input_funcs *funcs);
void *fh_get_input_user_data(struct fh_input *in);
int fh_close_input(struct fh_input *in);
ssize_t fh_input_read(struct fh_input *in, char *line, ssize_t max_len);

struct fh_program *fh_new_program(void);
void fh_free_program(struct fh_program *prog);
int fh_add_c_func(struct fh_program *prog, const char *name, fh_c_func func);
int fh_compile_file(struct fh_program *prog, const char *filename);
int fh_run_function(struct fh_program *prog, const char *func_name);
int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret);

const char *fh_get_error(struct fh_program *prog);
int fh_set_error(struct fh_program *prog, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap);

void fh_make_number(struct fh_value *val, double num);
void fh_make_c_func(struct fh_value *val, fh_c_func func);

#endif /* FH_H_FILE */
