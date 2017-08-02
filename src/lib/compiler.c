/* compiler.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"
#include "bytecode.h"

struct fh_compiler {
  struct fh_ast *ast;
  struct fh_bc *bc;
  uint8_t last_err_msg[256];
};

static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block);

struct fh_compiler *fh_new_compiler(struct fh_ast *ast, struct fh_bc *bc)
{
  struct fh_compiler *c = malloc(sizeof(struct fh_compiler));
  if (! c)
    return NULL;
  c->ast = ast;
  c->bc = bc;
  c->last_err_msg[0] = '\0';
  return c;
}

void fh_free_compiler(struct fh_compiler *c)
{
  free(c);
}

const uint8_t *fh_get_compiler_error(struct fh_compiler *c)
{
  return c->last_err_msg;
}

int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  snprintf((char *) c->last_err_msg, sizeof(c->last_err_msg), "%d:%d: %s", loc.line, loc.col, str);
  return -1;
}

static int add_instr(struct fh_compiler *c, struct fh_src_loc loc, uint32_t instr)
{
  if (! fh_bc_add_instr(c->bc, loc, instr))
    fh_compiler_error(c, loc, "out of memory for bytecode");
  return 0;
}

static int get_reg(struct fh_compiler *c)
{
  // TODO
  return 0;
}

static void free_reg(struct fh_compiler *c, int reg)
{
  // TODO
}

static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr, int dest_reg)
{
  if (dest_reg < 0) {
    dest_reg = get_reg(c);
    if (dest_reg < 0)
      return -1;
  }
    
  // TODO: compile expression setting value to reg
  return dest_reg;
}

static int compile_var_decl(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_decl *decl)
{
  int reg = get_reg(c);
  if (reg < 0)
    return -1;
  // TODO: variable 'decl->var' is now register 'reg'
  if (decl->val) {
    if (compile_expr(c, decl->val, reg) < 0)
      return -1;
  }
  return 0;
}

static int compile_return(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_return *ret)
{
  if (ret->val) {
    int reg = compile_expr(c, ret->val, -1);
    if (reg < 0)
      return -1;
    free_reg(c, reg);
    return add_instr(c, loc, MAKE_INSTR_ABC(OP_RET, 1, reg, 0));
  }
  return add_instr(c, loc, MAKE_INSTR_ABC(OP_RET, 0, 0, 0));
}

static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
  case STMT_EMPTY:
    return 0;
    
  case STMT_VAR_DECL:
    return compile_var_decl(c, stmt->loc, &stmt->data.decl);

  case STMT_EXPR:
    return compile_expr(c, stmt->data.expr, -1);

  case STMT_BLOCK:
    return compile_block(c, stmt->loc, &stmt->data.block);

  case STMT_RETURN:
    return compile_return(c, stmt->loc, &stmt->data.ret);
    
  case STMT_IF:
  case STMT_WHILE:
  case STMT_BREAK:
  case STMT_CONTINUE:
    return fh_compiler_error(c, stmt->loc, "compilation of this statement type not implemented\n");
  }

  return fh_compiler_error(c, stmt->loc, "invalid statement node type: %d\n", stmt->type);
}

static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block)
{
  // TODO: mark first free as [1]
  for (int i = 0; i < block->n_stmts; i++)
    if (compile_stmt(c, block->stmts[i]) < 0)
      return -1;
  // TODO: set first free reg to [1]
  return 0;
}

static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func)
{
  if (! fh_bc_add_func(c->bc, loc, func->n_params, 0)) {
    fh_compiler_error(c, loc, "out of memory");
    goto err;
  }

  return compile_block(c, loc, &func->body);
  
 err:
  return -1;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func)
{
  return compile_func(c, func->loc, &func->func);
}

int fh_compile(struct fh_compiler *c)
{
  for (int i = 0; i < c->ast->funcs.num; i++) {
    struct fh_p_named_func *f = fh_stack_item(&c->ast->funcs, i);
    if (compile_named_func(c, f) < 0)
      goto err;
  }
  return 0;

 err:
  return -1;
}
