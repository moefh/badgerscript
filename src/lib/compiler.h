/* compiler.h */

#ifndef COMPILER_H_FILE
#define COMPILER_H_FILE

#include "fh_i.h"
#include "stack.h"

struct fh_compiler {
  struct fh_program *prog;
  struct fh_ast *ast;
  struct fh_bc *bc;
  struct fh_stack funcs;
  struct fh_stack c_funcs;
};

int fh_init_compiler(struct fh_compiler *c, struct fh_program *prog);
void fh_destroy_compiler(struct fh_compiler *c);
int fh_compile(struct fh_compiler *c, struct fh_bc *bc, struct fh_ast *ast);
int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
int fh_compiler_add_c_func(struct fh_compiler *c, const char *name, fh_c_func func);
uint32_t *fh_get_compiler_instructions(struct fh_compiler *c, int32_t *len);

#endif /* COMPILER_H_FILE */
