/* compiler.h */

#ifndef COMPILER_H_FILE
#define COMPILER_H_FILE

#include "fh_i.h"
#include "stack.h"
#include "value.h"

DECLARE_STACK(int_stack, int);
DECLARE_STACK(code_stack, uint32_t);

struct reg_info {
  fh_symbol_id var;
  bool alloc;
};

DECLARE_STACK(reg_stack, struct reg_info);

struct func_info {
  struct func_info *parent;
  int num_regs;
  struct reg_stack regs;
  struct code_stack code;
  struct value_stack consts;

  int continue_target_addr;
  struct int_stack fix_break_addrs;
};

DECLARE_STACK(func_info_stack, struct func_info);

struct fh_compiler {
  struct fh_program *prog;
  struct fh_ast *ast;
  struct func_info_stack funcs;
};

void fh_init_compiler(struct fh_compiler *c, struct fh_program *prog);
void fh_destroy_compiler(struct fh_compiler *c);
int fh_compile(struct fh_compiler *c, struct fh_ast *ast);
int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
uint32_t *fh_get_compiler_instructions(struct fh_compiler *c, int32_t *len);

#endif /* COMPILER_H_FILE */
