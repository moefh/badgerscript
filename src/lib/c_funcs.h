/* c_funcs.h */

#ifndef C_FUNCS_H_FILE
#define C_FUNCS_H_FILE

#define DECL_FN(name) int fh_fn_##name(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)

DECL_FN(len);
DECL_FN(print);
DECL_FN(printf);

#endif /* C_FUNCS_H_FILE */
