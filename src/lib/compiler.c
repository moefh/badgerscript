/* compiler.c */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"
#include "bytecode.h"

#define TMP_VARIABLE ((fh_symbol_id)-1)

struct reg_info {
  fh_symbol_id var;
  bool alloc;
};

struct c_func_entry {
  const char *name;
  fh_c_func func;
};

struct func_info {
  struct fh_bc_func *bc_func;
  int num_regs;
  struct fh_stack regs;

  uint32_t continue_target_addr;
  struct fh_stack fix_break_addrs;
};

struct fh_compiler {
  struct fh_ast *ast;
  struct fh_bc *bc;
  char last_err_msg[256];
  struct fh_stack funcs;
  struct fh_stack c_funcs;
};

static void pop_func_info(struct fh_compiler *c);
static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block);
static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt);
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
  fh_init_stack(&c->c_funcs, sizeof(struct c_func_entry));
  return c;
}

void fh_free_compiler(struct fh_compiler *c)
{
  while (c->funcs.num > 0)
    pop_func_info(c);
  fh_free_stack(&c->funcs);
  fh_free_stack(&c->c_funcs);
  free(c);
}

int fh_compiler_add_c_func(struct fh_compiler *c, const char *name, fh_c_func func)
{
  struct c_func_entry *fe = fh_push(&c->c_funcs, NULL);
  if (! fe)
    return -1;
  fe->name = name;
  fe->func = func;
  return 0;
}

static struct func_info *new_func_info(struct fh_compiler *c, struct fh_bc_func *bc_func)
{
  struct func_info fi;
  fi.num_regs = 0;
  fi.bc_func = bc_func;
  fh_init_stack(&fi.regs, sizeof(struct reg_info));
  fi.continue_target_addr = (uint32_t) -1;
  fh_init_stack(&fi.fix_break_addrs, sizeof(uint32_t));

  if (! fh_push(&c->funcs, &fi))
    return NULL;
  return fh_stack_top(&c->funcs);
}

static uint32_t get_cur_pc(struct fh_compiler *c)
{
  uint32_t size;
  fh_get_bc_code(c->bc, &size);
  return size;
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
  fh_free_stack(&fi.fix_break_addrs);
}

const char *fh_get_compiler_error(struct fh_compiler *c)
{
  return c->last_err_msg;
}

const char *get_ast_symbol_name(struct fh_compiler *c, fh_symbol_id sym)
{
  const char *name = fh_get_ast_symbol(c->ast, sym);
  if (! name)
    name = "<INTERNAL COMPILER ERROR: UNKNOWN VARIABLE>";
  return name;
}

int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  snprintf(c->last_err_msg, sizeof(c->last_err_msg), "%d:%d: %s", loc.line, loc.col, str);
  return -1;
}

static int add_instr(struct fh_compiler *c, struct fh_src_loc loc, uint32_t instr)
{
  if (! fh_add_bc_instr(c->bc, loc, instr))
    fh_compiler_error(c, loc, "out of memory for bytecode");
  return 0;
}

static int set_jmp_target(struct fh_compiler *c, struct fh_src_loc loc, uint32_t instr_addr, uint32_t target_addr)
{
  int64_t diff = (int64_t) target_addr - (int64_t) instr_addr - 1;
  if (diff < -(1<<17) || diff > (1<<17))
    return fh_compiler_error(c, loc, "too far to jump (%u to %u)", instr_addr, target_addr);
  uint32_t cur_pc = get_cur_pc(c);
  if (instr_addr >= cur_pc)
    return fh_compiler_error(c, loc, "invalid instruction location (%u)", instr_addr);
  if (target_addr > cur_pc)
    return fh_compiler_error(c, loc, "invalid jump target location (%u)", target_addr);

  //printf("diff = %u - %u - 1 = %ld\n", target_addr, instr_addr, diff);
  
  uint32_t instr = fh_get_bc_instruction(c->bc, instr_addr);
  instr &= ~((uint32_t)0x3ffff<<14);
  instr |= PLACE_INSTR_RS(diff);
  fh_set_bc_instruction(c->bc, instr_addr, instr);
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
  const char *str_val = fh_get_ast_string(c->ast, str);
  if (! str_val)
    return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: string not found");

  int k = fh_add_bc_const_string(fi->bc_func, str_val);
  if (k < 0)
    return fh_compiler_error(c, loc, "out of memory for string");
  return k;
}

static int add_const_func(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id func)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  const char *name = fh_get_ast_symbol(c->ast, func);
  
  // bytecode function
  struct fh_bc_func *bc_func = fh_get_bc_func_by_name(c->bc, name);
  if (bc_func) {
    int k = fh_add_bc_const_func(fi->bc_func, bc_func);
    if (k < 0)
      return fh_compiler_error(c, loc, "out of memory for function constant");
    return k;
  }

  // C function
  stack_foreach(struct c_func_entry *, fe, &c->c_funcs) {
    if (strcmp(name, fe->name) == 0) {
      int k = fh_add_bc_const_c_func(fi->bc_func, fe->func);
      if (k < 0)
        return fh_compiler_error(c, loc, "out of memory for C function constant");
      return k;
    }
  }
  
  return fh_compiler_error(c, loc, "undefined function '%s'", get_ast_symbol_name(c, func));
}

static int alloc_reg(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  int new_reg = -1;
  int i = 0;
  stack_foreach(struct reg_info *, ri, &fi->regs) {
    if (! ri->alloc) {
      new_reg = i;
      break;
    }
    i++;
  }

  if (new_reg < 0) {
    new_reg = fi->regs.num;
    if (new_reg >= MAX_FUNC_REGS) {
      fh_compiler_error(c, loc, "too many registers used");
      return -1;
    }
    if (! fh_push(&fi->regs, NULL)) {
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
  if (! fi) {
    fprintf(stderr, "%s\n", fh_get_compiler_error(c));
    return;
  }

  struct reg_info *ri = fh_stack_item(&fi->regs, reg);
  if (! ri) {
    fprintf(stderr, "INTERNAL COMPILER ERROR: freeing invalid register (%d)\n", reg);
    return;
  }

  ri->alloc = false;
}

static int alloc_n_regs(struct fh_compiler *c, struct fh_src_loc loc, int n)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  int last_alloc = -1;
  for (int i = fi->regs.num-1; i >= 0; i--) {
    struct reg_info *ri = fh_stack_item(&fi->regs, i);
    if (ri->alloc) {
      last_alloc = i;
      break;
    }
  }

  int first_reg = last_alloc + 1;
  if (first_reg+n >= MAX_FUNC_REGS) {
    fh_compiler_error(c, loc, "too many registers used");
    return -1;
  }
  for (int i = 0; i < n; i++) {
    int reg = first_reg + i;
    if (fi->regs.num <= reg) {
      if (! fh_push(&fi->regs, NULL)) {
        fh_compiler_error(c, loc, "out of memory");
        return -1;
      }
    }
    struct reg_info *ri = fh_stack_item(&fi->regs, reg);
    ri->alloc = true;
    ri->var = TMP_VARIABLE;
  }

  if (fi->num_regs <= first_reg+n-1)
    fi->num_regs = first_reg+n;
  return first_reg;
}

static void free_tmp_regs(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return;

  stack_foreach(struct reg_info *, ri, &fi->regs) {
    if (ri->alloc && ri->var == TMP_VARIABLE)
      ri->alloc = false;
  }
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

  for (int i = fi->regs.num-1; i >= 0; i--) {
    struct reg_info *ri = fh_stack_item(&fi->regs, i);
    if (ri->alloc && ri->var == var)
      return i;
  }

  return fh_compiler_error(c, loc, "undeclared variable '%s'", get_ast_symbol_name(c, var));
}

static int get_opcode_for_op(struct fh_compiler *c, struct fh_src_loc loc, uint32_t op)
{
  switch (op) {
  case '+': return OPC_ADD;
  case '-': return OPC_SUB;
  case '*': return OPC_MUL;
  case '/': return OPC_DIV;
  case '%': return OPC_MOD;

  case AST_OP_UNM: return OPC_NEG;
  case '!':        return OPC_NOT;

  default:
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_ast_op(c->ast, op));
  }
}

static int compile_var(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var, int dest_reg)
{
  int reg = get_var_reg(c, loc, var);
  if (reg >= 0) {
    // local variable
    if (dest_reg < 0)
      return reg;
    if (reg != dest_reg) {
      if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, reg)) < 0)
        return -1;
    }
    return dest_reg;
  }

  int k = add_const_func(c, loc, var);
  if (k >= 0) {
    // global function
    if (dest_reg < 0) {
      dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
      if (dest_reg < 0)
        return -1;
    }
    if (add_instr(c, loc, MAKE_INSTR_AU(OPC_LDC, dest_reg, k)) < 0)
      return -1;
    return dest_reg;
  }

  return fh_compiler_error(c, loc, "unknown variable or function '%s'", get_ast_symbol_name(c, var));
}

static int check_is_var(struct fh_p_expr *expr, void *data)
{
  fh_symbol_id var = *(fh_symbol_id *)data;
  return (expr->type == EXPR_VAR && expr->data.var == var);
}

static int expr_contains_var(struct fh_p_expr *expr, fh_symbol_id var)
{
  return fh_ast_visit_expr_nodes(expr, check_is_var, &var);
}

#define expr_is_simple(e) ((e)->type == EXPR_VAR || (e)->type == EXPR_NUMBER || (e)->type == EXPR_STRING)

static int expr_is_simple_op(struct fh_p_expr *expr)
{
  if (expr->type == EXPR_BIN_OP && expr->data.bin_op.op != AST_OP_OR && expr->data.bin_op.op != AST_OP_AND) {
    return (expr_is_simple(expr->data.bin_op.left) && expr_is_simple(expr->data.bin_op.right));
  }
  if (expr->type == EXPR_UN_OP) {
    return expr_is_simple(expr->data.un_op.arg);
  }
  return 0;
}

static int compile_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr, int dest_reg)
{
  switch (expr->op) {
  case '=':
    if (expr->left->type == EXPR_VAR) {
      int left_reg = get_var_reg(c, loc, expr->left->data.var);
      if (left_reg < 0)
        return -1;
      /* Optimization: if expr->right is simple enough, we can
       * directly compile it using the assigned variable register as
       * the destination. Sadly this doesn't work in most cases where
       * expr->right contains the variable being assigned
       * (e.g. "y=(2*x)*y+c" fails to compile correctly with that
       * optimization).
       */
      if (expr_is_simple(expr->right)
          || expr_is_simple_op(expr->right)
          || ! expr_contains_var(expr->right, expr->left->data.var)) {
        if (compile_expr(c, expr->right, left_reg) < 0)
          return -1;
        if (dest_reg < 0)
          return left_reg;
        if (dest_reg != left_reg) {
          if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, left_reg)) < 0)
            return -1;
        }
      } else {
        //printf("%s <- ", fh_get_ast_symbol(c->ast, expr->left->data.var));
        //fh_dump_expr(c->ast, NULL, expr->right);
        //printf("\n");
        int tmp_reg = compile_expr(c, expr->right, dest_reg);
        if (tmp_reg < 0)
          return -1;
        if (tmp_reg != left_reg) {
          if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, left_reg, tmp_reg)) < 0)
            return -1;
        }
        if (dest_reg < 0)
          return left_reg;
      }
      return dest_reg;
    } else {
      return fh_compiler_error(c, loc, "invalid assignment");
    }

  case '+':
  case '-':
  case '*':
  case '/':
  case '%': {
    int left_reg;
    if (dest_reg >= 0 && expr->left->type == EXPR_VAR) {
      left_reg = get_var_reg(c, expr->left->loc, expr->left->data.var);
    } else if (expr->left->type == EXPR_NUMBER) {
      left_reg = add_const_number(c, loc, expr->left->data.num);
      if (left_reg >= 0) left_reg += MAX_FUNC_REGS+1;
    } else {
      left_reg = compile_expr(c, expr->left, dest_reg);
    }
    if (left_reg < 0)
      return -1;
    int right_reg;
    if (dest_reg >= 0 && expr->right->type == EXPR_VAR) {
      right_reg = get_var_reg(c, expr->left->loc, expr->right->data.var);
    } else if (expr->right->type == EXPR_NUMBER) {
      right_reg = add_const_number(c, loc, expr->right->data.num);
      if (right_reg >= 0) right_reg += MAX_FUNC_REGS+1;
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
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_ast_op(c->ast, expr->op));
  }
}

static int compile_un_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_un_op *expr, int dest_reg)
{
  switch (expr->op) {
  case '!':
  case AST_OP_UNM: {
    int reg;
    if (expr->arg->type == EXPR_VAR) {
      reg = get_var_reg(c, expr->arg->loc, expr->arg->data.var);
    } else if (expr->arg->type == EXPR_NUMBER) {
      reg = add_const_number(c, loc, expr->arg->data.num);
      if (reg >= 0) reg += MAX_FUNC_REGS+1;
      if (dest_reg < 0)
        dest_reg = reg;
    } else {
      reg = compile_expr(c, expr->arg, dest_reg);
    }
    if (reg < 0)
      return -1;
    int opcode = get_opcode_for_op(c, loc, expr->op);
    if (opcode < 0)
      return -1;
    if (dest_reg < 0) {
      dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
      if (dest_reg < 0)
        return -1;
    }
    if (add_instr(c, loc, MAKE_INSTR_AB(opcode, dest_reg, reg)) < 0)
      return -1;
    return dest_reg;
  }

  default:
    return fh_compiler_error(c, loc, "compilation for operator '%s' is not implemented", fh_get_ast_op(c->ast, expr->op));
  }
}

static int compile_func_call(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func_call *expr, int req_dest_reg)
{
  int func_reg = alloc_n_regs(c, loc, expr->n_args+1);
  if (func_reg < 0)
    return -1;
  if (compile_expr(c, expr->func, func_reg) < 0)
    return -1;
  for (int i = 0; i < expr->n_args; i++) {
    if (compile_expr(c, &expr->args[i], func_reg+i+1) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AB(OPC_CALL, func_reg, expr->n_args)) < 0)
    return -1;

  for (int i = 1; i < expr->n_args+1; i++)
    free_reg(c, loc, func_reg+i);

  if (req_dest_reg < 0)
    return func_reg;
  
  if (func_reg != req_dest_reg) {
    free_reg(c, loc, func_reg);
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, req_dest_reg, func_reg)) < 0)
      return -1;
  }
  return req_dest_reg;
}

static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr, int req_dest_reg)
{
  switch (expr->type) {
  case EXPR_VAR:       return compile_var(c, expr->loc, expr->data.var, req_dest_reg);
  case EXPR_BIN_OP:    return compile_bin_op(c, expr->loc, &expr->data.bin_op, req_dest_reg);
  case EXPR_UN_OP:     return compile_un_op(c, expr->loc, &expr->data.un_op, req_dest_reg);
  case EXPR_FUNC_CALL: return compile_func_call(c, expr->loc, &expr->data.func_call, req_dest_reg);
  case EXPR_FUNC:      return fh_compiler_error(c, expr->loc, "compilation of inner function implemented");
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
    if (add_instr(c, expr->loc, MAKE_INSTR_AU(OPC_LDC, dest_reg, k)) < 0)
      goto err;
    break;
  }

  case EXPR_STRING: {
    int k = add_const_string(c, expr->loc, expr->data.str);
    if (k < 0)
      goto err;
    if (add_instr(c, expr->loc, MAKE_INSTR_AU(OPC_LDC, dest_reg, k)) < 0)
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
  if (req_dest_reg < 0 && dest_reg >= 0)
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
    if (add_instr(c, loc, MAKE_INSTR_A(OPC_LD0, reg)) < 0)
      return -1;
  }
  if (set_reg_var(c, loc, reg, decl->var) < 0)
    return -1;
  return 0;
}

static int get_opcode_for_test(struct fh_compiler *c, struct fh_src_loc loc, uint32_t op, int *invert)
{
  switch (op) {
  case '<':        *invert = 0; return OPC_CMP_LT;
  case '>':        *invert = 1; return OPC_CMP_LE;
  case AST_OP_LE:  *invert = 0; return OPC_CMP_LE;
  case AST_OP_GE:  *invert = 1; return OPC_CMP_LT;
  case AST_OP_EQ:  *invert = 0; return OPC_CMP_EQ;
  case AST_OP_NEQ: *invert = 1; return OPC_CMP_EQ;
    
  default:
    return fh_compiler_error(c, loc, "invalid operator for test: '%u", op);
  }
}

static int compile_test(struct fh_compiler *c, struct fh_p_expr *test, int invert_test)
{
  if (test->type == EXPR_UN_OP && test->data.un_op.op == '!') {
    invert_test = ! invert_test;
    test = test->data.un_op.arg;
  }
  
  if (test->type == EXPR_BIN_OP) {
    switch (test->data.bin_op.op) {
    case '>':
    case '<':
    case AST_OP_GE:
    case AST_OP_LE:
    case AST_OP_EQ:
    case AST_OP_NEQ: {
      int left_reg;
      if (test->data.bin_op.left->type == EXPR_NUMBER) {
        left_reg = add_const_number(c, test->data.bin_op.left->loc, test->data.bin_op.left->data.num);
        if (left_reg >= 0) left_reg += MAX_FUNC_REGS+1;
      } else {
        left_reg = compile_expr(c, test->data.bin_op.left, -1);
      }
      if (left_reg < 0)
        return -1;
      int right_reg;
      if (test->data.bin_op.right->type == EXPR_NUMBER) {
        right_reg = add_const_number(c, test->data.bin_op.right->loc, test->data.bin_op.right->data.num);
        if (right_reg >= 0) right_reg += MAX_FUNC_REGS+1;
      } else {
        right_reg = compile_expr(c, test->data.bin_op.right, -1);
      }
      if (right_reg < 0)
        return -1;
      int invert = 0;
      int opcode = get_opcode_for_test(c, test->loc, test->data.bin_op.op, &invert);
      if (opcode < 0)
        return -1;
      if (add_instr(c, test->loc, MAKE_INSTR_ABC(opcode, invert_test ^ invert, left_reg, right_reg)) < 0)
        return -1;
      return 0;
    }

    default:
      break;
    }
  }

  int reg;
  if (test->type == EXPR_NUMBER) {
    reg = add_const_number(c, test->loc, test->data.num);
    if (reg >= 0) reg += MAX_FUNC_REGS+1;
  } else {
    reg = compile_expr(c, test, -1);
  }
  if (reg < 0)
    return -1;
  if (add_instr(c, test->loc, MAKE_INSTR_AB(OPC_TEST, reg, invert_test)) < 0)
    return -1;
  return 0;
}

static int compile_if(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_if *stmt_if)
{
  if (compile_test(c, stmt_if->test, 0) < 0)
    return -1;
  free_tmp_regs(c, loc);

  // jmp to_false
  uint32_t addr_jmp_to_false = get_cur_pc(c);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;

  if (compile_stmt(c, stmt_if->true_stmt) < 0)
    return -1;

  // jmp to_end
  uint32_t addr_jmp_to_end = get_cur_pc(c);
  if (stmt_if->false_stmt) {
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
      return -1;
  }
  // to_false:
  if (set_jmp_target(c, loc, addr_jmp_to_false, get_cur_pc(c)) < 0)
    return -1;
  if (stmt_if->false_stmt) {
    if (compile_stmt(c, stmt_if->false_stmt) < 0)
      return -1;
    // to_end:
    if (set_jmp_target(c, loc, addr_jmp_to_end, get_cur_pc(c)) < 0)
      return -1;
  }
  return 0;
}

static int compile_while(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_while *stmt_while)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  uint32_t save_continue_target_addr = fi->continue_target_addr;
  int num_fix_break_addrs = fi->fix_break_addrs.num;
  
  fi->continue_target_addr = get_cur_pc(c);
  if (compile_test(c, stmt_while->test, 0) < 0)
    return -1;
  free_tmp_regs(c, loc);

  // jmp to_end
  uint32_t addr_jmp_to_end = get_cur_pc(c);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;

  if (compile_stmt(c, stmt_while->stmt) < 0)
    return -1;
  free_tmp_regs(c, loc);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, fi->continue_target_addr-get_cur_pc(c)-1)) < 0)
    return -1;

  // to_end:
  uint32_t addr_end = get_cur_pc(c);
  if (set_jmp_target(c, loc, addr_jmp_to_end, addr_end) < 0)
    return -1;

  while (fi->fix_break_addrs.num > num_fix_break_addrs) {
    uint32_t break_addr;
    if (fh_pop(&fi->fix_break_addrs, &break_addr) < 0)
      return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: can't pop break address");
    if (set_jmp_target(c, loc, break_addr, addr_end) < 0)
      return -1;
  }
  fi->continue_target_addr = save_continue_target_addr;
  return 0;
}

static int compile_break(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  uint32_t break_addr = get_cur_pc(c);
  if (! fh_push(&fi->fix_break_addrs, &break_addr))
    return fh_compiler_error(c, loc, "out of memory");
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;
  return 0;
}

static int compile_continue(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  if (fi->continue_target_addr == (uint32_t) -1) {
    fh_compiler_error(c, loc, "'continue' not inside 'while'");
    return -1;
  }
  
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, fi->continue_target_addr-get_cur_pc(c)-1)) < 0)
    return -1;
  return 0;
}

static int compile_return(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_return *ret)
{
  if (ret->val) {
    int reg = compile_expr(c, ret->val, -1);
    if (reg < 0)
      return -1;
    return add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, reg, 1));
  }
  return add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, 0, 0));
}

static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
  case STMT_EMPTY:
    return 0;
    
  case STMT_VAR_DECL:
    if (compile_var_decl(c, stmt->loc, &stmt->data.decl) < 0)
      return -1;
    free_tmp_regs(c, stmt->loc);
    return 0;

  case STMT_EXPR:
    if (compile_expr(c, stmt->data.expr, -1) < 0)
      return -1;
    free_tmp_regs(c, stmt->loc);
    return 0;

  case STMT_BLOCK:
    return compile_block(c, stmt->loc, &stmt->data.block);

  case STMT_RETURN:
    if (compile_return(c, stmt->loc, &stmt->data.ret))
      return -1;
    free_tmp_regs(c, stmt->loc);
    return 0;
    
  case STMT_IF:
    return compile_if(c, stmt->loc, &stmt->data.stmt_if);
    
  case STMT_WHILE:
    return compile_while(c, stmt->loc, &stmt->data.stmt_while);
    
  case STMT_BREAK:
    return compile_break(c, stmt->loc);
    
  case STMT_CONTINUE:
    return compile_continue(c, stmt->loc);
  }

  return fh_compiler_error(c, stmt->loc, "invalid statement node type: %d\n", stmt->type);
}

static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  struct fh_stack save_regs;
  fh_init_stack(&save_regs, fi->regs.item_size);
  if (fh_copy_stack(&save_regs, &fi->regs) < 0)
    return fh_compiler_error(c, loc, "out of memory for block regs");
  
  for (int i = 0; i < block->n_stmts; i++)
    if (compile_stmt(c, block->stmts[i]) < 0)
      return -1;

  if (fh_copy_stack(&fi->regs, &save_regs) < 0)
    return fh_compiler_error(c, loc, "out of memory for block regs");
  fh_free_stack(&save_regs);
  return 0;
}

static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func, struct fh_bc_func *bc_func)
{
  struct func_info *fi = new_func_info(c, bc_func);
  if (! fi) {
    fh_compiler_error(c, loc, "out of memory");
    return -1;
  }

  bc_func->addr = get_cur_pc(c);
 
  for (int i = 0; i < func->n_params; i++) {
    if (alloc_reg(c, loc, func->params[i]) < 0)
      return -1;
  }

  if (compile_block(c, loc, &func->body) < 0)
    return -1;

  if (func->body.n_stmts == 0 || func->body.stmts[func->body.n_stmts-1]->type != STMT_RETURN) {
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, 0, 0)) < 0)
      return -1;
  }
  
  bc_func->n_opc = get_cur_pc(c) - bc_func->addr;
  bc_func->n_regs = fi->num_regs;
  pop_func_info(c);
  return 0;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func, struct fh_bc_func *bc_func)
{
  if (compile_func(c, func->loc, &func->func, bc_func) < 0)
    return -1;

  if (c->funcs.num > 0)
    return fh_compiler_error(c, func->loc, "INTERNAL COMPILER ERROR: function info was not cleared");

  return 0;
}

static const char *get_func_name(struct fh_compiler *c, struct fh_p_named_func *f)
{
  const char *name = fh_get_ast_symbol(c->ast, f->name);
  if (! name) {
    fh_compiler_error(c, f->loc, "INTERNAL COMPILER ERROR: can't find function name");
    return NULL;
  }
  return name;
}

int fh_compile(struct fh_compiler *c)
{
  stack_foreach(struct fh_p_named_func *, f, &c->ast->funcs) {
    const char *name = get_func_name(c, f);
    if (! name)
      return -1;
    if (! fh_add_bc_func(c->bc, f->loc, name, f->func.n_params))
      return -1;
  }

  stack_foreach(struct fh_p_named_func *, f, &c->ast->funcs) {
    const char *name = get_func_name(c, f);
    if (! name)
      return -1;
    struct fh_bc_func *bc_func = fh_get_bc_func_by_name(c->bc, name);
    if (compile_named_func(c, f, bc_func) < 0)
      return -1;
  }

  return 0;
}
