/* compiler.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"
#include "bytecode.h"

#define TMP_VARIABLE ((fh_symbol_id)-1)
#define MAX_FUNC_REGS 255

struct reg_info {
  fh_symbol_id var;
  bool alloc;
};

struct func_info {
  struct fh_bc_func *bc_func;
  int num_regs;
  struct fh_stack regs;
};

struct fh_compiler {
  struct fh_ast *ast;
  struct fh_bc *bc;
  uint8_t last_err_msg[256];
  struct fh_stack funcs;
};

static void pop_func_info(struct fh_compiler *c);
static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block);
static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr, int req_dest_reg);

struct fh_compiler *fh_new_compiler(struct fh_ast *ast, struct fh_bc *bc)
{
  struct fh_compiler *c = malloc(sizeof(struct fh_compiler));
  if (! c)
    return NULL;
  c->ast = ast;
  c->bc = bc;
  c->last_err_msg[0] = '\0';
  fh_init_stack(&c->funcs, sizeof(struct func_info));
  return c;
}

void fh_free_compiler(struct fh_compiler *c)
{
  while (c->funcs.num > 0)
    pop_func_info(c);
  fh_free_stack(&c->funcs);
  free(c);
}

static struct func_info *new_func_info(struct fh_compiler *c, struct fh_bc_func *bc_func)
{
  struct func_info fi;
  fi.num_regs = 0;
  fi.bc_func = bc_func;
  fh_init_stack(&fi.regs, sizeof(struct reg_info));

  if (fh_push(&c->funcs, &fi) < 0)
    return NULL;
  return fh_stack_top(&c->funcs);
}

static struct func_info *get_cur_func_info(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = fh_stack_top(&c->funcs);
  if (! fi)
    fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: no current function");
  return fi;
}

static void pop_func_info(struct fh_compiler *c)
{
  struct func_info fi;
  if (fh_pop(&c->funcs, &fi) < 0)
    return;
  fh_free_stack(&fi.regs);
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
  if (! fh_add_bc_instr(c->bc, loc, instr))
    fh_compiler_error(c, loc, "out of memory for bytecode");
  return 0;
}

static int add_const_number(struct fh_compiler *c, struct fh_src_loc loc, double num)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int k = fh_add_bc_const_number(fi->bc_func, num);
  if (k < 0)
    return fh_compiler_error(c, loc, "too many constants in function");
  return k;
}

static int add_const_string(struct fh_compiler *c, struct fh_src_loc loc, fh_string_id str)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  const  uint8_t *str_val = fh_get_ast_string(c->ast, str);
  if (! str_val)
    return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: string not found");
  int k = fh_add_bc_const_string(fi->bc_func, str_val);
  if (k < 0)
    return fh_compiler_error(c, loc, "out of memory for string");
  return k;
}

static int alloc_reg(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  int new_reg = -1;
  for (int i = 0; i < fi->regs.num; i++) {
    struct reg_info *ri = fh_stack_item(&fi->regs, i);
    if (! ri->alloc) {
      new_reg = i;
      break;
    }
  }

  if (new_reg < 0) {
    new_reg = fi->regs.num;
    if (new_reg >= MAX_FUNC_REGS) {
      fh_compiler_error(c, loc, "too many registers used");
      return -1;
    }
    if (fh_push(&fi->regs, NULL) < 0) {
      fh_compiler_error(c, loc, "out of memory");
      return -1;
    }
  }

  struct reg_info *ri = fh_stack_item(&fi->regs, new_reg);
  ri->var = var;
  ri->alloc = true;

  if (fi->num_regs <= new_reg)
    fi->num_regs = new_reg+1;
  return new_reg;
}

static void free_reg(struct fh_compiler *c, struct fh_src_loc loc, int reg)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return;

  struct reg_info *ri = fh_stack_item(&fi->regs, reg);
  if (! ri) // || ! ri->alloc)
    fprintf(stderr, "INTERNAL COMPILER ERROR: freeing invalid register (%d)\n", reg);

  ri->alloc = false;
}

static int set_reg_var(struct fh_compiler *c, struct fh_src_loc loc, int reg, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  struct reg_info *ri = fh_stack_item(&fi->regs, reg);
  if (! ri)
    return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: unknown register %d", reg);
  ri->var = var;
  return 0;
}

static int get_var_reg(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  for (int i = 0; i < fi->regs.num; i++) {
    struct reg_info *ri = fh_stack_item(&fi->regs, i);
    if (ri->alloc && ri->var == var)
      return i;
  }

  const uint8_t *name = fh_get_ast_symbol(c->ast, var);
  if (! name)
    name = (uint8_t *) "<INTERNAL COMPILER ERROR: UNKNOWN VARIABLE>";
  return fh_compiler_error(c, loc, "undeclared variable '%s'", name);
}

static int get_opcode_for_op(struct fh_compiler *c, struct fh_src_loc loc, uint32_t op)
{
  switch (op) {
  case '+': return OP_ADD;
  case '-': return OP_SUB;
  case '*': return OP_MUL;
  case '/': return OP_DIV;

  default:
    return fh_compiler_error(c, loc, "operator not implemented: '%s'", fh_get_ast_op(c->ast, op));
  }
}

static int compile_var(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var, int dest_reg)
{
  int reg = get_var_reg(c, loc, var);
  if (reg < 0)
    return -1;
  if (dest_reg < 0)
    return reg;
  if (reg != dest_reg) {
    if (add_instr(c, loc, MAKE_INSTR_AB(OP_MOV, dest_reg, reg)) < 0)
      return -1;
  }
  return dest_reg;
}

static int compile_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr, int dest_reg)
{
  switch (expr->op) {
  case '=':
    if (expr->left->type == EXPR_VAR) {
      int left_reg = get_var_reg(c, loc, expr->left->data.var);
      if (left_reg < 0)
        return -1;
      if (compile_expr(c, expr->right, left_reg) < 0)
        return -1;
      if (dest_reg < 0)
        return left_reg;
      if (dest_reg != left_reg) {
        if (add_instr(c, loc, MAKE_INSTR_AB(OP_MOV, dest_reg, left_reg)) < 0)
          return -1;
      }
      return dest_reg;
    } else {
      return fh_compiler_error(c, loc, "invalid assignment");
    }

  case '+':
  case '-':
  case '*':
  case '/': {
    int left_reg;
    if (dest_reg >= 0 && expr->left->type == EXPR_VAR) {
      left_reg = get_var_reg(c, expr->left->loc, expr->left->data.var);
    } else {
      left_reg = compile_expr(c, expr->left, dest_reg);
    }
    if (left_reg < 0)
      return -1;
    int right_reg;
    if (dest_reg >= 0 && expr->right->type == EXPR_VAR) {
      right_reg = get_var_reg(c, expr->left->loc, expr->right->data.var);
    } else {
      right_reg = compile_expr(c, expr->right, -1);
    }
    if (right_reg < 0)
      return -1;
    int opcode = get_opcode_for_op(c, loc, expr->op);
    if (opcode < 0)
      return -1;
    if (dest_reg < 0) {
      dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
      if (dest_reg < 0)
        return -1;
    }
    if (add_instr(c, loc, MAKE_INSTR_ABC(opcode, dest_reg, left_reg, right_reg)) < 0)
      return -1;
    return dest_reg;
  }

  default:
    return fh_compiler_error(c, loc, "operator '%s' not implemented", fh_get_ast_op(c->ast, expr->op));
  }
}

static int compile_un_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_un_op *expr, int req_dest_reg)
{
  return fh_compiler_error(c, loc, "un op not implemented");
}

static int compile_func_call(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func_call *expr, int req_dest_reg)
{
  return fh_compiler_error(c, loc, "func call not implemented");
}

static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr, int req_dest_reg)
{
  switch (expr->type) {
  case EXPR_VAR:       return compile_var(c, expr->loc, expr->data.var, req_dest_reg);
  case EXPR_BIN_OP:    return compile_bin_op(c, expr->loc, &expr->data.bin_op, req_dest_reg);
  case EXPR_UN_OP:     return compile_un_op(c, expr->loc, &expr->data.un_op, req_dest_reg);
  case EXPR_FUNC_CALL: return compile_func_call(c, expr->loc, &expr->data.func_call, req_dest_reg);
  case EXPR_FUNC:      /* return compile_func(c, expr->loc, &expr->data.func, req_dest_reg); */ return fh_compiler_error(c, expr->loc, "function-in-function not implemented");
  default:
    break;
  }

  int dest_reg = req_dest_reg;
  if (dest_reg < 0) {
    dest_reg = alloc_reg(c, expr->loc, TMP_VARIABLE);
    if (dest_reg < 0)
      return -1;
  }

  switch (expr->type) {
  case EXPR_NUMBER: {
    int k = add_const_number(c, expr->loc, expr->data.num);
    if (k < 0)
      goto err;
    if (add_instr(c, expr->loc, MAKE_INSTR_ABx(OP_LOADK, dest_reg, k)) < 0)
      goto err;
    break;
  }

  case EXPR_STRING: {
    int k = add_const_string(c, expr->loc, expr->data.str);
    if (k < 0)
      goto err;
    if (add_instr(c, expr->loc, MAKE_INSTR_ABx(OP_LOADK, dest_reg, k)) < 0)
      goto err;
    break;
 }
    
  case EXPR_NONE:
  case EXPR_VAR:
  case EXPR_BIN_OP:
  case EXPR_UN_OP:
  case EXPR_FUNC_CALL:
  case EXPR_FUNC:
    break;
  }
  return dest_reg;

 err:
  if (req_dest_reg < 0 && dest_reg >= req_dest_reg)
    free_reg(c, expr->loc, dest_reg);
  return -1;
}

static int compile_var_decl(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_decl *decl)
{
  int reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (reg < 0)
    return -1;
  if (decl->val) {
    if (compile_expr(c, decl->val, reg) < 0)
      return -1;
  } else {
    if (add_instr(c, loc, MAKE_INSTR_A(OP_LOAD0, reg)) < 0)
      return -1;
  }
  if (set_reg_var(c, loc, reg, decl->var) < 0)
    return -1;
  return 0;
}

static int compile_return(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_return *ret)
{
  if (ret->val) {
    int reg = compile_expr(c, ret->val, -1);
    if (reg < 0)
      return -1;
    free_reg(c, loc, reg);
    return add_instr(c, loc, MAKE_INSTR_AB(OP_RET, 1, reg));
  }
  return add_instr(c, loc, MAKE_INSTR_AB(OP_RET, 0, 0));
}

static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
  case STMT_EMPTY:
    return 0;
    
  case STMT_VAR_DECL:
    return compile_var_decl(c, stmt->loc, &stmt->data.decl);

  case STMT_EXPR: {
    // TODO: free regs used by expr compilation
    return compile_expr(c, stmt->data.expr, -1);
  }

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
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int max_reg = fi->regs.num;
  
  for (int i = 0; i < block->n_stmts; i++)
    if (compile_stmt(c, block->stmts[i]) < 0)
      return -1;
  
  for (int i = max_reg; i < fi->regs.num; i++)
    free_reg(c, loc, i);
  return 0;
}

static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func)
{
  struct fh_bc_func *bc_func = fh_add_bc_func(c->bc, loc, func->n_params, 0);
  if (! bc_func) {
    fh_compiler_error(c, loc, "out of memory");
    return -1;
  }
  struct func_info *fi = new_func_info(c, bc_func);
  if (! fi) {
    fh_compiler_error(c, loc, "out of memory");
    return -1;
  }

  for (int i = 0; i < func->n_params; i++) {
    if (alloc_reg(c, loc, func->params[i]) < 0)
      return -1;
  }
  
  if (compile_block(c, loc, &func->body) < 0)
    return -1;

  if (add_instr(c, loc, MAKE_INSTR_AB(OP_RET, 0, 0)) < 0)
    return -1;
  
  bc_func->n_regs = fi->num_regs;
  pop_func_info(c);
  return 0;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func)
{
  if (compile_func(c, func->loc, &func->func) < 0)
    return -1;

  if (c->funcs.num > 0)
    return fh_compiler_error(c, func->loc, "INTERNAL COMPILER ERROR: function info was not cleared");

  return 0;
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
