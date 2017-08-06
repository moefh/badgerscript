/* fh.h */

#ifndef FH_H_FILE
#define FH_H_FILE

#include <stdlib.h>
#include <stdint.h>

struct fh_input;

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
struct fh_bc;
struct fh_vm;

typedef int (*fh_c_func)(struct fh_vm *vm, struct fh_value *ret, struct fh_value *args, int n_args);

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

struct fh_bc *fh_new_bc(void);
void fh_free_bc(struct fh_bc *bc);

struct fh_vm *fh_new_vm(struct fh_bc *bc);
void fh_free_vm(struct fh_vm *vm);
int fh_vm_error(struct fh_vm *vm, char *fmt, ...);
const char *fh_get_vm_error(struct fh_vm *vm);
int fh_call_function(struct fh_vm *vm, const char *name, struct fh_value *args, int n_args, struct fh_value *ret);

#endif /* FH_H_FILE */
