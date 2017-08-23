/* compiler.h */

#ifndef COMPILER_H_FILE
#define COMPILER_H_FILE

#include "fh_internal.h"
#include "stack.h"
#include "value.h"

DECLARE_STACK(code_stack, uint32_t);
DECLARE_STACK(upval_def_stack, struct fh_upval_def);

enum compiler_block_type {
  COMP_BLOCK_PLAIN,
  COMP_BLOCK_FUNC,
  COMP_BLOCK_WHILE,
};

struct block_info {
  struct block_info *parent;
  enum compiler_block_type type;
  int32_t start_addr;
  int parent_num_regs;
};

struct reg_info {
  struct reg_info *next;
  int reg;
  fh_symbol_id var;
  bool alloc;
  bool used_by_inner_func;
};

struct break_addr {
  struct break_addr *next;
  int address;
  int num;
};

struct func_info {
  struct func_info *parent;
  int num_regs;
  struct reg_info *reg_list;
  struct break_addr *break_addr_list;
  struct block_info *block_list;
  struct code_stack code;
  struct value_stack consts;
  struct upval_def_stack upvals;
  struct fh_buffer code_src_loc;
  struct fh_src_loc last_instr_src_loc;
};

struct fh_compiler {
  struct fh_program *prog;
  struct fh_mem_pool *pool;
  struct fh_ast *ast;
  struct func_info *func_info;
};

void fh_init_compiler(struct fh_compiler *c, struct fh_program *prog, struct fh_mem_pool *pool);
void fh_destroy_compiler(struct fh_compiler *c);
int fh_compile(struct fh_compiler *c, struct fh_ast *ast);
int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...) FH_PRINTF_FORMAT(3, 4);
uint32_t *fh_get_compiler_instructions(struct fh_compiler *c, int32_t *len);

#endif /* COMPILER_H_FILE */
