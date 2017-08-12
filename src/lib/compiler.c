/* compiler.c */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "compiler.h"
#include "stack.h"
#include "ast.h"
#include "bytecode.h"

#define TMP_VARIABLE     ((fh_symbol_id)-1)
#define MAX_FUNC_CONSTS  (512-MAX_FUNC_REGS)

#define RK_IS_CONST(rk)   ((rk)>=MAX_FUNC_REGS+1)

static void pop_func_info(struct fh_compiler *c);
static int compile_expr_to_reg(struct fh_compiler *c, struct fh_p_expr *expr, int dest_reg);
static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block);
static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt);
static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr);
static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func, struct fh_func *bc_func, struct func_info *parent);

void fh_init_compiler(struct fh_compiler *c, struct fh_program *prog)
{
  c->prog = prog;
  c->ast = NULL;
  func_info_stack_init(&c->funcs);
}

static void reset_compiler(struct fh_compiler *c)
{
  c->ast = NULL;
  while (func_info_stack_size(&c->funcs) > 0)
    pop_func_info(c);
}

void fh_destroy_compiler(struct fh_compiler *c)
{
  reset_compiler(c);
  func_info_stack_free(&c->funcs);
}

static struct func_info *new_func_info(struct fh_compiler *c, struct func_info *parent)
{
  struct func_info *fi = func_info_stack_push(&c->funcs, NULL);
  if (! fi)
    return NULL;

  fi->parent = parent;
  fi->num_regs = 0;
  code_stack_init(&fi->code);
  value_stack_init(&fi->consts);
  reg_stack_init(&fi->regs);
  fi->continue_target_addr = -1;
  int_stack_init(&fi->fix_break_addrs);

  return func_info_stack_top(&c->funcs);
}

static struct func_info *get_cur_func_info(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = func_info_stack_top(&c->funcs);
  if (! fi)
    fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: no current function");
  return fi;
}

static void pop_func_info(struct fh_compiler *c)
{
  struct func_info fi;
  if (func_info_stack_pop(&c->funcs, &fi) < 0)
    return;
  reg_stack_free(&fi.regs);
  int_stack_free(&fi.fix_break_addrs);
  code_stack_free(&fi.code);
  value_stack_free(&fi.consts);
}

static int get_cur_pc(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  return code_stack_size(&fi->code);
}

static const char *get_ast_symbol_name(struct fh_compiler *c, fh_symbol_id sym)
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

  fh_set_error(c->prog, "%d:%d: %s", loc.line, loc.col, str);
  return -1;
}

static struct fh_func *new_func(struct fh_compiler *c, struct fh_src_loc loc, const char *name, int n_params)
{
  UNUSED(loc); // TODO: record source location

  struct fh_func *func = fh_make_func(c->prog);
  if (! func)
    return NULL;
  if (! name)
    func->name = NULL;
  else {
    func->name = fh_make_string(c->prog, name);
    if (! func->name)
      return NULL;
  }
  func->n_params = n_params;
  func->n_regs = 0;
  func->code = NULL;
  func->code_size = 0;
  func->consts = NULL;
  func->n_consts = 0;
  return func;
}

static int add_instr(struct fh_compiler *c, struct fh_src_loc loc, uint32_t instr)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  if (! code_stack_push(&fi->code, &instr))
    return fh_compiler_error(c, loc, "out of memory for bytecode");
  return 0;
}

static int set_jmp_target(struct fh_compiler *c, struct fh_src_loc loc, int instr_addr, int target_addr)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  int diff = target_addr - instr_addr - 1;
  if (diff < -(1<<17) || diff > (1<<17))
    return fh_compiler_error(c, loc, "too far to jump (%u to %u)", instr_addr, target_addr);
  int cur_pc = get_cur_pc(c, loc);
  if (target_addr > cur_pc)
    return fh_compiler_error(c, loc, "invalid jump target location (%d)", target_addr);

  //printf("diff = %u - %u - 1 = %ld\n", target_addr, instr_addr, diff);

  uint32_t *p_instr = code_stack_item(&fi->code, instr_addr);
  if (! p_instr)
    return fh_compiler_error(c, loc, "invalid instruction location (%d)", instr_addr);
  *p_instr &= ~((uint32_t)0x3ffff<<14);
  *p_instr |= PLACE_INSTR_RS(diff);
  return 0;
}

static struct fh_value *add_const(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return NULL;

  if (value_stack_size(&fi->consts) >= MAX_FUNC_CONSTS) {
    fh_compiler_error(c, loc, "too many constants in function");
    return NULL;
  }

  struct fh_value *val = value_stack_push(&fi->consts, NULL);
  if (! val) {
    fh_compiler_error(c, loc, "out of memory");
    return NULL;
  }
  return val;
}

static int add_const_number(struct fh_compiler *c, struct fh_src_loc loc, double num)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int k = 0;
  stack_foreach(struct fh_value, *, c, &fi->consts) {
    if (c->type == FH_VAL_NUMBER && c->data.num == num)
      return k;
    k++;
  }

  k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! val)
    return -1;
  val->type = FH_VAL_NUMBER;
  val->data.num = num;
  return k;
}

static int add_const_string(struct fh_compiler *c, struct fh_src_loc loc, fh_string_id str_id)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int k = 0;
  const char *str = fh_get_ast_string(c->ast, str_id);
  stack_foreach(struct fh_value, *, c, &fi->consts) {
    if (c->type == FH_VAL_STRING && strcmp(fh_get_string(c), str) == 0)
      return k;
    k++;
  }
  
  k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! c)
    return -1;
  struct fh_object *obj = fh_make_string(c->prog, str);
  if (! obj) {
    value_stack_pop(&fi->consts, NULL);
    return -1;
  }
  val->type = FH_VAL_STRING;
  val->data.obj = obj;
  return k;
}

static int add_const_global_func(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id func)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  const char *name = fh_get_ast_symbol(c->ast, func);
  
  // bytecode function
  struct fh_func *bc_func = fh_get_global_func(c->prog, name);
  if (bc_func) {
    int k = 0;
    stack_foreach(struct fh_value, *, c, &fi->consts) {
      if (c->type == FH_VAL_FUNC && c->data.obj == bc_func)
        return k;
      k++;
    }

    k = value_stack_size(&fi->consts);
    struct fh_value *val = add_const(c, loc);
    if (! val)
      return -1;
    val->type = FH_VAL_FUNC;
    val->data.obj = bc_func;
    return k;
  }

  // C function
  fh_c_func c_func = fh_get_c_func_by_name(c->prog, name);
  if (c_func) {
    int k = 0;
    stack_foreach(struct fh_value, *, c, &fi->consts) {
      if (c->type == FH_VAL_C_FUNC && c->data.c_func == c_func)
        return k;
      k++;
    }
    k = value_stack_size(&fi->consts);
    struct fh_value *val = add_const(c, loc);
    if (! val)
      return -1;
    val->type = FH_VAL_C_FUNC;
    val->data.c_func = c_func;
    return k;
  }
  
  return fh_compiler_error(c, loc, "undefined function '%s'", get_ast_symbol_name(c, func));
}

static int add_const_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_func *func)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! c)
    return -1;
  val->type = FH_VAL_FUNC;
  val->data.obj = func;
  return k;
}

static int alloc_reg(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  int new_reg = -1;
  int i = 0;
  stack_foreach(struct reg_info, *, ri, &fi->regs) {
    if (! ri->alloc) {
      new_reg = i;
      break;
    }
    i++;
  }

  if (new_reg < 0) {
    new_reg = reg_stack_size(&fi->regs);
    if (new_reg >= MAX_FUNC_REGS) {
      fh_compiler_error(c, loc, "too many registers used");
      return -1;
    }
    if (! reg_stack_push(&fi->regs, NULL)) {
      fh_compiler_error(c, loc, "out of memory");
      return -1;
    }
  }

  struct reg_info *ri = reg_stack_item(&fi->regs, new_reg);
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
    fprintf(stderr, "%s\n", fh_get_error(c->prog));
    return;
  }

  struct reg_info *ri = reg_stack_item(&fi->regs, reg);
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
  for (int i = reg_stack_size(&fi->regs)-1; i >= 0; i--) {
    struct reg_info *ri = reg_stack_item(&fi->regs, i);
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
    if (reg_stack_size(&fi->regs) <= reg) {
      if (! reg_stack_push(&fi->regs, NULL)) {
        fh_compiler_error(c, loc, "out of memory");
        return -1;
      }
    }
    struct reg_info *ri = reg_stack_item(&fi->regs, reg);
    ri->alloc = true;
    ri->var = TMP_VARIABLE;
  }

  if (fi->num_regs <= first_reg+n-1)
    fi->num_regs = first_reg+n;
  return first_reg;
}

#if 0
static bool reg_is_tmp(struct fh_compiler *c, struct fh_src_loc loc, int reg)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi) {
    fprintf(stderr, "%s\n", fh_get_error(c->prog));
    return false;
  }

  struct reg_info *ri = reg_stack_item(&fi->regs, reg);
  if (! ri) {
    fprintf(stderr, "INTERNAL COMPILER ERROR: invalid register (%d)\n", reg);
    return false;
  }

  return ri->var == TMP_VARIABLE;
}
#endif

static void free_tmp_regs(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return;

  stack_foreach(struct reg_info, *, ri, &fi->regs) {
    if (ri->alloc && ri->var == TMP_VARIABLE)
      ri->alloc = false;
  }
}

static int set_reg_var(struct fh_compiler *c, struct fh_src_loc loc, int reg, fh_symbol_id var)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;

  struct reg_info *ri = reg_stack_item(&fi->regs, reg);
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

  for (int i = reg_stack_size(&fi->regs)-1; i >= 0; i--) {
    struct reg_info *ri = reg_stack_item(&fi->regs, i);
    if (ri->alloc && ri->var == var)
      return i;
  }

  return fh_compiler_error(c, loc, "undeclared variable '%s'", get_ast_symbol_name(c, var));
}

static int compile_var(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  // local variable
  int reg = get_var_reg(c, loc, var);
  if (reg >= 0)
    return reg;

  // global function
  int k = add_const_global_func(c, loc, var);
  if (k >= 0)
    return k + MAX_FUNC_REGS + 1;

  return fh_compiler_error(c, loc, "unknown variable or function '%s'", get_ast_symbol_name(c, var));
}

#if 0
static int check_is_var(struct fh_p_expr *expr, void *data)
{
  fh_symbol_id var = *(fh_symbol_id *)data;
  return (expr->type == EXPR_VAR && expr->data.var == var);
}

static int expr_contains_var(struct fh_p_expr *expr, fh_symbol_id var)
{
  return fh_ast_visit_expr_nodes(expr, check_is_var, &var);
}
#endif

static int compile_bin_op_to_reg(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr, int dest_reg)
{
  if (expr->op == AST_OP_AND || expr->op == AST_OP_OR) {
    if (compile_expr_to_reg(c, expr->left, dest_reg) < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_TEST, expr->op == AST_OP_OR, dest_reg)) < 0)
      return -1;
    int jmp_addr = get_cur_pc(c, loc);
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
      return -1;
    if (compile_expr_to_reg(c, expr->right, dest_reg) < 0)
      return -1;
    if (set_jmp_target(c, loc, jmp_addr, get_cur_pc(c, loc)) < 0)
      return -1;
    return dest_reg;
  }
  
  int left_rk = compile_expr(c, expr->left);
  if (left_rk < 0)
    return -1;

  int right_rk = compile_expr(c, expr->right);
  if (right_rk < 0)
    return -1;

  enum fh_bc_opcode opc;
  switch (expr->op) {
  case '+': opc = OPC_ADD; break;
  case '-': opc = OPC_SUB; break;
  case '*': opc = OPC_MUL; break;
  case '/': opc = OPC_DIV; break;
  case '%': opc = OPC_MOD; break;
  default:
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_ast_op(c->ast, expr->op));
  }
  if (add_instr(c, loc, MAKE_INSTR_ABC(opc, dest_reg, left_rk, right_rk)) < 0)
    return -1;

  return dest_reg;
}

static int compile_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr)
{
  if (expr->op == '=') {
    if (expr->left->type == EXPR_VAR) {
      int left_reg = get_var_reg(c, loc, expr->left->data.var);
      if (left_reg < 0)
        return -1;
      return compile_expr_to_reg(c, expr->right, left_reg);
    }

    if (expr->left->type == EXPR_INDEX) {
      int container_rk = compile_expr(c, expr->left->data.index.container);
      if (container_rk < 0)
        return -1;
      if (RK_IS_CONST(container_rk)) {  // container must be a register (RA)
        int tmp_reg = alloc_reg(c, loc, TMP_VARIABLE);
        if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, tmp_reg, container_rk)) < 0)
          return -1;
        container_rk = tmp_reg;
      }

      int index_rk = compile_expr(c, expr->left->data.index.index);
      if (index_rk < 0)
        return -1;
      int val_rk = compile_expr(c, expr->right);
      if (val_rk < 0)
        return -1;

      if (add_instr(c, loc, MAKE_INSTR_ABC(OPC_SETEL, container_rk, index_rk, val_rk)) < 0)
        return -1;
      return val_rk;
    }
    
    return fh_compiler_error(c, loc, "invalid assignment");
  }

  int dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (dest_reg < 0)
    return -1;
  return compile_bin_op_to_reg(c, loc, expr, dest_reg);
}

static int compile_un_op_to_reg(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_un_op *expr, int dest_reg)
{
  int arg_rk = compile_expr(c, expr->arg);
  if (arg_rk < 0)
    return -1;

  enum fh_bc_opcode opc;
  switch (expr->op) {
  case AST_OP_UNM: opc = OPC_NEG; break;
  case '!':        opc = OPC_NOT; break;
  default:
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_ast_op(c->ast, expr->op));
  }
  if (add_instr(c, loc, MAKE_INSTR_AB(opc, dest_reg, arg_rk)) < 0)
    return -1;
  return dest_reg;
}

static int compile_un_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_un_op *expr)
{
  int dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (dest_reg < 0)
    return -1;
  return compile_un_op_to_reg(c, loc, expr, dest_reg);
}

static int compile_func_call(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func_call *expr)
{
  int func_reg = alloc_n_regs(c, loc, expr->n_args+1);
  if (func_reg < 0)
    return -1;
  if (compile_expr_to_reg(c, expr->func, func_reg) < 0)
    return -1;
  for (int i = 0; i < expr->n_args; i++) {
    if (compile_expr_to_reg(c, &expr->args[i], func_reg + 1 + i) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AB(OPC_CALL, func_reg, expr->n_args)) < 0)
    return -1;

  for (int i = 1; i < expr->n_args+1; i++)
    free_reg(c, loc, func_reg+i);

  return func_reg;
}

static int compile_index_to_reg(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_index *expr, int dest_reg)
{
  int container_rk = compile_expr(c, expr->container);
  if (container_rk < 0)
    return -1;
  
  int index_rk = compile_expr(c, expr->index);
  if (index_rk < 0)
    return -1;

  if (add_instr(c, loc, MAKE_INSTR_ABC(OPC_GETEL, dest_reg, container_rk, index_rk)) < 0)
    return -1;
  return dest_reg;
}

static int compile_index(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_index *expr)
{
  int dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (dest_reg < 0)
    return -1;
  return compile_index_to_reg(c, loc, expr, dest_reg);
}

static int compile_array_lit(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_array_lit *expr)
{
  int array_reg = alloc_n_regs(c, loc, expr->n_elems+1);
  if (array_reg < 0)
    return -1;
  for (int i = 0; i < expr->n_elems; i++) {
    if (compile_expr_to_reg(c, &expr->elems[i], array_reg + 1 + i) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AU(OPC_NEWARRAY, array_reg, expr->n_elems)) < 0)
    return -1;

  for (int i = 1; i < expr->n_elems+1; i++)
    free_reg(c, loc, array_reg+i);

  return array_reg;
}

static int compile_inner_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *expr)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  struct fh_func *func = new_func(c, loc, NULL, expr->n_params);
  if (! func || fh_add_func(c->prog, func, false) < 0)
    return -1;
  if (compile_func(c, loc, expr, func, fi) < 0)
    return -1;
  int k = add_const_func(c, loc, func);
  if (k >= 0)
    k += MAX_FUNC_REGS + 1;
  return k;
}

static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_VAR:       return compile_var(c, expr->loc, expr->data.var);
  case EXPR_BIN_OP:    return compile_bin_op(c, expr->loc, &expr->data.bin_op);
  case EXPR_UN_OP:     return compile_un_op(c, expr->loc, &expr->data.un_op);
  case EXPR_FUNC_CALL: return compile_func_call(c, expr->loc, &expr->data.func_call);
  case EXPR_ARRAY_LIT: return compile_array_lit(c, expr->loc, &expr->data.array_lit);
  case EXPR_INDEX:     return compile_index(c, expr->loc, &expr->data.index);
  case EXPR_FUNC:      return compile_inner_func(c, expr->loc, &expr->data.func);

  case EXPR_NUMBER:
    {
      int k = add_const_number(c, expr->loc, expr->data.num);
      if (k >= 0)
        k += MAX_FUNC_REGS + 1;
      return k;
    }
    
  case EXPR_STRING:
    {
      int k = add_const_string(c, expr->loc, expr->data.str);
      if (k >= 0)
        k += MAX_FUNC_REGS + 1;
      return k;
    }

  case EXPR_NONE:
    if (add_instr(c, expr->loc, MAKE_INSTR_AB(OPC_MOV, 0, 0)) < 0) // NOP
      return -1;
    return 0;
  }

  return fh_compiler_error(c, expr->loc, "INTERNAL COMPILER ERROR: invalid expression type %d", expr->type);
}

static int compile_expr_to_reg(struct fh_compiler *c, struct fh_p_expr *expr, int dest_reg)
{
  switch (expr->type) {
  case EXPR_BIN_OP:    return compile_bin_op_to_reg(c, expr->loc, &expr->data.bin_op, dest_reg);
  case EXPR_UN_OP:     return compile_un_op_to_reg(c, expr->loc, &expr->data.un_op, dest_reg);
  case EXPR_INDEX:     return compile_index_to_reg(c, expr->loc, &expr->data.index, dest_reg);
  default: break;
  }
  
  int tmp_rk = compile_expr(c, expr);
  if (tmp_rk < 0)
    return -1;
  if (tmp_rk <= MAX_FUNC_REGS) {
    if (add_instr(c, expr->loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, tmp_rk)) < 0)
      return -1;
  } else {
    if (add_instr(c, expr->loc, MAKE_INSTR_AB(OPC_LDC, dest_reg, tmp_rk - MAX_FUNC_REGS - 1)) < 0)
      return -1;
  }
  return dest_reg;
}

static int compile_var_decl(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_decl *decl)
{
  int reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (reg < 0)
    return -1;
  if (decl->val) {
    if (compile_expr_to_reg(c, decl->val, reg) < 0)
      return -1;
  } else {
    if (add_instr(c, loc, MAKE_INSTR_A(OPC_LDNULL, reg)) < 0)
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
      int left_rk = compile_expr(c, test->data.bin_op.left);
      if (left_rk < 0)
        return -1;
      int right_rk = compile_expr(c, test->data.bin_op.right);
      if (right_rk < 0)
        return -1;
      int invert = 0;
      int opcode = get_opcode_for_test(c, test->loc, test->data.bin_op.op, &invert);
      if (opcode < 0)
        return -1;
      if (add_instr(c, test->loc, MAKE_INSTR_ABC(opcode, invert_test ^ invert, left_rk, right_rk)) < 0)
        return -1;
      return 0;
    }

    default:
      break;
    }
  }

  int rk;
  if (test->type == EXPR_NUMBER) {
    rk = add_const_number(c, test->loc, test->data.num);
    if (rk >= 0) rk += MAX_FUNC_REGS+1;
  } else {
    rk = compile_expr(c, test);
  }
  if (rk < 0)
    return -1;
  if (add_instr(c, test->loc, MAKE_INSTR_AB(OPC_TEST, invert_test, rk)) < 0)
    return -1;
  return 0;
}

static int compile_if(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_if *stmt_if)
{
  if (compile_test(c, stmt_if->test, 0) < 0)
    return -1;
  free_tmp_regs(c, loc);

  // jmp to_false
  int addr_jmp_to_false = get_cur_pc(c, loc);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;

  if (compile_stmt(c, stmt_if->true_stmt) < 0)
    return -1;

  // jmp to_end
  int addr_jmp_to_end = get_cur_pc(c, loc);
  if (stmt_if->false_stmt) {
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
      return -1;
  }
  // to_false:
  if (set_jmp_target(c, loc, addr_jmp_to_false, get_cur_pc(c, loc)) < 0)
    return -1;
  if (stmt_if->false_stmt) {
    if (compile_stmt(c, stmt_if->false_stmt) < 0)
      return -1;
    // to_end:
    if (set_jmp_target(c, loc, addr_jmp_to_end, get_cur_pc(c, loc)) < 0)
      return -1;
  }
  return 0;
}

static int compile_while(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_while *stmt_while)
{
  struct func_info *fi = get_cur_func_info(c, loc);
  if (! fi)
    return -1;
  int save_continue_target_addr = fi->continue_target_addr;
  int num_fix_break_addrs = int_stack_size(&fi->fix_break_addrs);
  
  fi->continue_target_addr = get_cur_pc(c, loc);
  if (compile_test(c, stmt_while->test, 0) < 0)
    return -1;
  free_tmp_regs(c, loc);

  // jmp to_end
  int addr_jmp_to_end = get_cur_pc(c, loc);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;

  if (compile_stmt(c, stmt_while->stmt) < 0)
    return -1;
  free_tmp_regs(c, loc);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, fi->continue_target_addr-get_cur_pc(c, loc)-1)) < 0)
    return -1;

  // to_end:
  int addr_end = get_cur_pc(c, loc);
  if (set_jmp_target(c, loc, addr_jmp_to_end, addr_end) < 0)
    return -1;

  while (int_stack_size(&fi->fix_break_addrs) > num_fix_break_addrs) {
    int break_addr;
    if (int_stack_pop(&fi->fix_break_addrs, &break_addr) < 0)
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

  int break_addr = get_cur_pc(c, loc);
  if (! int_stack_push(&fi->fix_break_addrs, &break_addr))
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
  if (fi->continue_target_addr < 0) {
    fh_compiler_error(c, loc, "'continue' not inside 'while'");
    return -1;
  }
  
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, fi->continue_target_addr-get_cur_pc(c, loc)-1)) < 0)
    return -1;
  return 0;
}

static int compile_return(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_return *ret)
{
  if (ret->val) {
    int tmp_rk = compile_expr(c, ret->val);
    if (tmp_rk < 0)
      return -1;
    return add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, 1, tmp_rk));
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
    if (compile_expr(c, stmt->data.expr) < 0)
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

  struct reg_stack save_regs;
  reg_stack_init(&save_regs);
  if (reg_stack_copy(&save_regs, &fi->regs) < 0)
    return fh_compiler_error(c, loc, "out of memory for block regs");
  
  int ret = 0;
  for (int i = 0; i < block->n_stmts; i++)
    if (compile_stmt(c, block->stmts[i]) < 0) {
      ret = -1;
      break;
    }

  if (reg_stack_copy(&fi->regs, &save_regs) < 0)
    return fh_compiler_error(c, loc, "out of memory for block regs");
  reg_stack_free(&save_regs);
  return ret;
}

static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func, struct fh_func *bc_func, struct func_info *parent)
{
  struct func_info *fi = new_func_info(c, parent);
  if (! fi) {
    fh_compiler_error(c, loc, "out of memory");
    goto err;
  }

  for (int i = 0; i < func->n_params; i++) {
    if (alloc_reg(c, loc, func->params[i]) < 0)
      goto err;
  }

  if (compile_block(c, loc, &func->body) < 0)
    goto err;

  if (func->body.n_stmts == 0 || func->body.stmts[func->body.n_stmts-1]->type != STMT_RETURN) {
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, 0, 0)) < 0)
      goto err;
  }

  if (code_stack_shrink_to_fit(&fi->code) < 0 || value_stack_shrink_to_fit(&fi->consts) < 0) {
    fh_compiler_error(c, loc, "out of memory");
    goto err;
  }
    
  bc_func->n_regs = fi->num_regs;
  
  bc_func->code_size = code_stack_size(&fi->code);
  bc_func->code = code_stack_data(&fi->code);
  code_stack_init(&fi->code);
  
  bc_func->n_consts = value_stack_size(&fi->consts);
  bc_func->consts = value_stack_data(&fi->consts);
  value_stack_init(&fi->consts);
  
  pop_func_info(c);
  return 0;

 err:
  pop_func_info(c);
  return -1;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func, struct fh_func *bc_func)
{
  if (compile_func(c, func->loc, &func->func, bc_func, NULL) < 0)
    return -1;

  if (func_info_stack_size(&c->funcs) > 0)
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

int fh_compile(struct fh_compiler *c, struct fh_ast *ast)
{
  reset_compiler(c);
  c->ast = ast;
  
  stack_foreach(struct fh_p_named_func, *, f, &c->ast->funcs) {
    const char *name = get_func_name(c, f);
    if (! name)
      return -1;
    
    if (fh_get_global_func(c->prog, name)) {
      fh_compiler_error(c, f->loc, "function '%s' already exists", name);
      return -1;
    }
    struct fh_func *func = new_func(c, f->loc, name, f->func.n_params);
    if (! func || fh_add_func(c->prog, func, true) < 0) {
      fh_compiler_error(c, f->loc, "out of memory");
      return -1;
    }
  }

  stack_foreach(struct fh_p_named_func, *, f, &c->ast->funcs) {
    const char *name = get_func_name(c, f);
    if (! name)
      return -1;
    struct fh_func *func = fh_get_global_func(c->prog, name);
    if (! func) {
      fh_compiler_error(c, f->loc, "INTERNAL COMPILER ERROR: can't find function '%s'", name);
      return -1;
    }
    if (compile_named_func(c, f, func) < 0)
      return -1;
  }

  return 0;
}
