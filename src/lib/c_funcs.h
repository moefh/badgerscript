/* c_funcs.h */

#ifndef C_FUNCS_H_FILE
#define C_FUNCS_H_FILE

int fh_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);
int fh_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);
int fh_len(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

#endif /* C_FUNCS_H_FILE */
