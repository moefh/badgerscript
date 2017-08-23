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
static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block, enum compiler_block_type block_type, int loop_start_addr);
static int compile_stmt(struct fh_compiler *c, struct fh_p_stmt *stmt);
static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr);
static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func, struct fh_func_def *func_def);
static int compile_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr);
static int compile_test_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *bin_op, bool invert_test);

void fh_init_compiler(struct fh_compiler *c, struct fh_program *prog, struct fh_mem_pool *pool)
{
  c->prog = prog;
  c->pool = pool;
  c->ast = NULL;
  c->func_info = NULL;
}

static void reset_compiler(struct fh_compiler *c)
{
  c->ast = NULL;
  while (c->func_info != NULL)
    pop_func_info(c);
}

void fh_destroy_compiler(struct fh_compiler *c)
{
  reset_compiler(c);
  while (c->func_info != NULL)
    pop_func_info(c);
}

static struct func_info *new_func_info(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = fh_malloc(c->pool, sizeof(struct func_info));
  if (! fi) {
    fh_compiler_error(c, loc, "out of memory");
    return NULL;
  }
  fi->parent = c->func_info;
  c->func_info = fi;
  
  fi->num_regs = 0;
  fi->reg_list = NULL;
  fi->block_list = NULL;
  fi->break_addr_list = NULL;
  fi->last_instr_src_loc = loc;
  
  code_stack_init(&fi->code);
  value_stack_init(&fi->consts);
  upval_def_stack_init(&fi->upvals);
  fh_init_buffer(&fi->code_src_loc, NULL);
  
  return fi;
}

static void pop_func_info(struct fh_compiler *c)
{
  struct func_info *fi = c->func_info;
  c->func_info = fi->parent;
  
  code_stack_free(&fi->code);
  value_stack_free(&fi->consts);
  upval_def_stack_free(&fi->upvals);
  fh_destroy_buffer(&fi->code_src_loc);
}

static struct block_info *new_block_info(struct fh_compiler *c)
{
  struct block_info *bi = fh_malloc(c->pool, sizeof(struct block_info));
  if (! bi)
    return NULL;
  bi->parent = c->func_info->block_list;
  c->func_info->block_list = bi;

  bi->type = COMP_BLOCK_PLAIN;
  bi->start_addr = -1;
  bi->parent_num_regs = 0;

  return bi;
}

static struct break_addr *new_break_address(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct break_addr *ba = fh_malloc(c->pool, sizeof(struct break_addr));
  if (! ba) {
    fh_compiler_error(c, loc, "out of memory");
    return NULL;
  }
  ba->next = c->func_info->break_addr_list;
  c->func_info->break_addr_list = ba;
  
  ba->num = (ba->next) ? ba->next->num+1 : 0;
  return ba;
}

static struct block_info *get_cur_block_info(struct fh_compiler *c, enum compiler_block_type type)
{
  for (struct block_info *bi = c->func_info->block_list; bi != NULL; bi = bi->parent) {
    if (type == bi->type)
      return bi;
  }
  return NULL;
}

static int get_cur_pc(struct fh_compiler *c)
{
  return code_stack_size(&c->func_info->code);
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

  fh_set_error(c->prog, "%s:%d:%d: %s", fh_get_ast_file_name(c->ast, loc.file_id), loc.line, loc.col, str);
  return -1;
}

static struct fh_func_def *new_func_def(struct fh_compiler *c, struct fh_src_loc loc, const char *name, int n_params)
{
  UNUSED(loc); // TODO: record source location

  struct fh_func_def *func_def = fh_make_func_def(c->prog, true);
  if (! func_def)
    return NULL;
  func_def->name = NULL;
  func_def->n_params = n_params;
  func_def->n_regs = 0;
  func_def->code = NULL;
  func_def->code_size = 0;
  func_def->consts = NULL;
  func_def->n_consts = 0;
  func_def->upvals = NULL;
  func_def->n_upvals = 0;
  func_def->code_src_loc = NULL;
  func_def->code_src_loc_size = 0;
  if (name) {
    func_def->name = fh_make_string(c->prog, true, name);
    if (! func_def->name)
      return NULL;
  }
  return func_def;
}

static int add_instr(struct fh_compiler *c, struct fh_src_loc loc, uint32_t instr)
{
  struct func_info *fi = c->func_info;
  if (! code_stack_push(&fi->code, &instr))
    return fh_compiler_error(c, loc, "out of memory for bytecode");

  if (fh_encode_src_loc_change(&fi->code_src_loc, &fi->last_instr_src_loc, &loc) < 0)
    return fh_compiler_error(c, loc, "out of memory for bytecode source location");
  fi->last_instr_src_loc = loc;
  return 0;
}

static int set_jmp_target(struct fh_compiler *c, struct fh_src_loc loc, int instr_addr, int target_addr)
{
  struct func_info *fi = c->func_info;

  int diff = target_addr - instr_addr - 1;
  if (diff < -(1<<17) || diff > (1<<17))
    return fh_compiler_error(c, loc, "too far to jump (%u to %u)", instr_addr, target_addr);
  int cur_pc = get_cur_pc(c);
  if (target_addr > cur_pc)
    return fh_compiler_error(c, loc, "invalid jump target location (%d)", target_addr);

  //printf("diff = %u - %u - 1 = %ld\n", target_addr, instr_addr, diff);

  uint32_t *p_instr = code_stack_item(&fi->code, instr_addr);
  if (! p_instr)
    return fh_compiler_error(c, loc, "invalid instruction location (%d)", instr_addr);
  *p_instr &= ~INSTR_RS_MASK;
  *p_instr |= PLACE_INSTR_RS(diff);
  return 0;
}

static struct fh_value *add_const(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = c->func_info;

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

static int add_const_null(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct func_info *fi = c->func_info;
  int k = 0;
  stack_foreach(struct fh_value, *, c, &fi->consts) {
    if (c->type == FH_VAL_NULL)
      return k;
    k++;
  }

  k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! val)
    return -1;
  val->type = FH_VAL_NULL;
  return k;
}

static int add_const_bool(struct fh_compiler *c, struct fh_src_loc loc, bool b)
{
  struct func_info *fi = c->func_info;
  int k = 0;
  stack_foreach(struct fh_value, *, c, &fi->consts) {
    if (c->type == FH_VAL_BOOL && c->data.b == b)
      return k;
    k++;
  }

  k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! val)
    return -1;
  val->type = FH_VAL_BOOL;
  val->data.b = b;
  return k;
}

static int add_const_number(struct fh_compiler *c, struct fh_src_loc loc, double num)
{
  struct func_info *fi = c->func_info;
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
  struct func_info *fi = c->func_info;
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
  struct fh_string *str_obj = fh_make_string(c->prog, true, str);
  if (! str_obj) {
    value_stack_pop(&fi->consts, NULL);
    return -1;
  }
  val->type = FH_VAL_STRING;
  val->data.obj = str_obj;
  return k;
}

static int add_const_global_func(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id func)
{
  struct func_info *fi = c->func_info;

  const char *name = fh_get_ast_symbol(c->ast, func);
  
  // closure
  struct fh_closure *closure = fh_get_global_func_by_name(c->prog, name);
  if (closure) {
    int k = 0;
    stack_foreach(struct fh_value, *, c, &fi->consts) {
      if (c->type == FH_VAL_CLOSURE && c->data.obj == closure)
        return k;
      k++;
    }

    k = value_stack_size(&fi->consts);
    struct fh_value *val = add_const(c, loc);
    if (! val)
      return -1;
    val->type = FH_VAL_CLOSURE;
    val->data.obj = closure;
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

static int add_const_func_def(struct fh_compiler *c, struct fh_src_loc loc, struct fh_func_def *func_def)
{
  struct func_info *fi = c->func_info;
  int k = value_stack_size(&fi->consts);
  struct fh_value *val = add_const(c, loc);
  if (! c)
    return -1;
  val->type = FH_VAL_FUNC_DEF;
  val->data.obj = func_def;
  return k;
}

static int add_upval(struct fh_compiler *c, struct fh_src_loc loc, struct func_info *fi, enum fh_upval_def_type type, int num)
{
  int upval = 0;
  stack_foreach(struct fh_upval_def, *, uv, &fi->upvals) {
    if (uv->type == type && num == uv->num)
      return upval;
    upval++;
  }

  upval = upval_def_stack_size(&fi->upvals);
  struct fh_upval_def *uv = upval_def_stack_push(&fi->upvals, NULL);
  if (! uv) {
    fh_compiler_error(c, loc, "out of memory");
    return -1;
  }
  uv->type = type;
  uv->num = num;
  return upval;
}

static struct reg_info *make_new_reg(struct fh_compiler *c, struct fh_src_loc loc, int reg)
{
  struct reg_info *ri = fh_malloc(c->pool, sizeof(struct reg_info));
  if (! ri) {
    fh_compiler_error(c, loc, "out of memory");
    return NULL;
  }
  ri->next = c->func_info->reg_list;
  ri->reg = reg;
  c->func_info->reg_list = ri;
  c->func_info->num_regs = ri->reg+1;
  return ri;
}

static int alloc_reg(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  int new_reg = 0;
  struct reg_info *ri;
  for (ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (! ri->alloc)
      break;
    new_reg++;
  }

  if (! ri) {
    if (new_reg >= MAX_FUNC_REGS)
      return fh_compiler_error(c, loc, "too many registers used");
    ri = make_new_reg(c, loc, new_reg);
    if (! ri)
      return -1;
  }

  ri->var = var;
  ri->alloc = true;
  ri->used_by_inner_func = false;
  return ri->reg;
}

static struct reg_info *get_reg_info(struct func_info *fi, int reg)
{
  for (struct reg_info *ri = fi->reg_list; ri != NULL; ri = ri->next)
    if (ri->reg == reg)
      return ri;
  return NULL;
}

static void free_reg(struct fh_compiler *c, int reg)
{
  struct reg_info *ri = get_reg_info(c->func_info, reg);
  if (! ri) {
    fprintf(stderr, "INTERNAL COMPILER ERROR: freeing invalid register (%d)\n", reg);
    return;
  }

  ri->alloc = false;
}

static int alloc_n_regs(struct fh_compiler *c, struct fh_src_loc loc, int n)
{
  int n_regs_to_alloc = n;
  int last_alloc = -1;
  for (struct reg_info *ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (ri->alloc) {
      last_alloc = ri->reg;
      break;
    }
    n_regs_to_alloc--;
  }

  int first_reg = last_alloc + 1;
  if (first_reg+n >= MAX_FUNC_REGS)
    return fh_compiler_error(c, loc, "too many registers used");

  for (int i = 0; i < n; i++) {
    int reg = first_reg + i;
    struct reg_info *ri;
    if (n_regs_to_alloc >= n - i) {
      ri = make_new_reg(c, loc, reg);
      if (! ri)
        return fh_compiler_error(c, loc, "out of memory");
    } else
      ri = get_reg_info(c->func_info, reg);
    ri->var = TMP_VARIABLE;
    ri->alloc = true;
    ri->used_by_inner_func = false;
  }
  return first_reg;
}

static void free_tmp_regs(struct fh_compiler *c)
{
  for (struct reg_info *ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (ri->alloc && ri->var == TMP_VARIABLE)
      ri->alloc = false;
  }
}

static void free_var_regs(struct fh_compiler *c, int first_var_reg)
{
  for (struct reg_info *ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (ri->reg < first_var_reg)
      break;
    if (ri->alloc && ri->var != TMP_VARIABLE)
      ri->alloc = false;
  }
}

static int set_reg_var(struct fh_compiler *c, struct fh_src_loc loc, int reg, fh_symbol_id var)
{
  struct reg_info *ri = get_reg_info(c->func_info, reg);
  if (! ri)
    return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: unknown register %d", reg);
  ri->var = var;
  return 0;
}

static int get_func_var_reg(struct func_info *fi, fh_symbol_id var)
{
  for (struct reg_info *ri = fi->reg_list; ri != NULL; ri = ri->next) {
    if (ri->alloc && ri->var == var)
      return ri->reg;
  }
  return -1;
}

static int get_var_reg(struct fh_compiler *c, fh_symbol_id var)
{
  return get_func_var_reg(c->func_info, var);
}

static int add_func_var_upval(struct fh_compiler *c, struct fh_src_loc loc, struct func_info *fi, fh_symbol_id var, int *ret_upval)
{
  if (! fi->parent) {
    *ret_upval = -1;
    return 0;
  }
  
  int reg = get_func_var_reg(fi->parent, var);
  if (reg >= 0) {
    struct reg_info *ri = get_reg_info(fi->parent, reg);
    ri->used_by_inner_func = true;
    
    int upval = add_upval(c, loc, fi, FH_UPVAL_TYPE_REG, reg);
    if (upval < 0)
      return -1;
    *ret_upval = upval;
    return 0;
  }
    
  if (add_func_var_upval(c, loc, fi->parent, var, ret_upval) < 0)
    return -1;
  if (*ret_upval < 0)
    return 0;
  int upval = add_upval(c, loc, fi, FH_UPVAL_TYPE_UPVAL, *ret_upval);
  if (upval < 0)
    return -1;
  *ret_upval = upval;
  return 0;
}

static int add_var_upval(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var, int *ret_upval)
{
  return add_func_var_upval(c, loc, c->func_info, var, ret_upval);
}

static int get_top_var_reg(struct fh_compiler *c)
{
  for (struct reg_info *ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (ri->alloc && ri->var != TMP_VARIABLE)
      return ri->reg;
  }
  return -1;
}

static int get_num_open_upvals(struct fh_compiler *c, struct fh_src_loc loc, int first_var_reg)
{
  int num_open_upvals = 0;
  for (struct reg_info *ri = c->func_info->reg_list; ri != NULL; ri = ri->next) {
    if (ri->reg < first_var_reg)
      break;
    if (ri->alloc && ri->used_by_inner_func) {
      if (ri->var == TMP_VARIABLE)
        return fh_compiler_error(c, loc, "INTERNAL COMPILER ERROR: tmp reg used by inner function?");
      num_open_upvals++;
    }
  }
  return num_open_upvals;
}

static int compile_var(struct fh_compiler *c, struct fh_src_loc loc, fh_symbol_id var)
{
  // local variable
  int reg = get_var_reg(c, var);
  if (reg >= 0)
    return reg;

  // parent function variable
  int upval;
  if (add_var_upval(c, loc, var, &upval) < 0)
    return -1;
  if (upval >= 0) {
    int reg = alloc_reg(c, loc, TMP_VARIABLE);
    if (reg < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_GETUPVAL, reg, upval)) < 0)
      return -1;
    return reg;
  }
  
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

static bool is_test_bin_op(struct fh_p_expr_bin_op *expr)
{
  switch (expr->op) {
  case '>':
  case '<':
  case AST_OP_GE:
  case AST_OP_LE:
  case AST_OP_EQ:
  case AST_OP_NEQ:
    return true;

  default:
    return false;
  }
}
 
static int compile_bin_op_to_reg(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr, int dest_reg)
{
  if (expr->op == '=') {
    int reg = compile_bin_op(c, loc, expr);
    if (reg < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, reg)) < 0)
      return -1;
    return dest_reg;
  }

  if (is_test_bin_op(expr)) {
    int k_true = add_const_bool(c, loc, true);
    int k_false = add_const_bool(c, loc, false);
    if (k_true < 0 || k_false < 0)
      return -1;
    k_true += MAX_FUNC_REGS + 1;
    k_false += MAX_FUNC_REGS + 1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, k_true)) < 0)
      return -1;
    if (compile_test_bin_op(c, loc, expr, false) < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_MOV, dest_reg, k_false)) < 0)
      return -1;
    return dest_reg;
  }
  
  if (expr->op == AST_OP_AND || expr->op == AST_OP_OR) {
    if (compile_expr_to_reg(c, expr->left, dest_reg) < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_TEST, expr->op == AST_OP_OR, dest_reg)) < 0)
      return -1;
    int jmp_addr = get_cur_pc(c);
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
      return -1;
    if (compile_expr_to_reg(c, expr->right, dest_reg) < 0)
      return -1;
    if (set_jmp_target(c, loc, jmp_addr, get_cur_pc(c)) < 0)
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
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_op_name(expr->op));
  }
  if (add_instr(c, loc, MAKE_INSTR_ABC(opc, dest_reg, left_rk, right_rk)) < 0)
    return -1;

  return dest_reg;
}

static int compile_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *expr)
{
  if (expr->op == '=') {
    if (expr->left->type == EXPR_VAR) {
      // local var
      int left_reg = get_var_reg(c, expr->left->data.var);
      if (left_reg >= 0)
        return compile_expr_to_reg(c, expr->right, left_reg);
      
      // var local to parent function
      int upval;
      if (add_var_upval(c, loc, expr->left->data.var, &upval) < 0)
        return -1;
      if (upval >= 0) {
        left_reg = alloc_reg(c, loc, TMP_VARIABLE);
        if (left_reg < 0)
          return -1;
        if (compile_expr_to_reg(c, expr->right, left_reg) < 0)
          return -1;
        if (add_instr(c, loc, MAKE_INSTR_AB(OPC_SETUPVAL, upval, left_reg)) < 0)
          return -1;
        return left_reg;
      }

      // no such variable
      return fh_compiler_error(c, expr->left->loc, "undeclared variable: '%s'", fh_get_ast_symbol(c->ast, expr->left->data.var));
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
    return fh_compiler_error(c, loc, "compilation of operator '%s' is not implemented", fh_get_op_name(expr->op));
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
  int n_args = fh_expr_list_size(expr->arg_list);
  int func_reg = alloc_n_regs(c, loc, n_args + 1);
  if (func_reg < 0)
    return -1;
  if (compile_expr_to_reg(c, expr->func, func_reg) < 0)
    return -1;
  int reg = func_reg + 1;
  for (struct fh_p_expr *e = expr->arg_list; e != NULL; e = e->next) {
    if (compile_expr_to_reg(c, e, reg++) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AB(OPC_CALL, func_reg, n_args)) < 0)
    return -1;

  for (int i = 1; i < n_args+1; i++)
    free_reg(c, func_reg+i);

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
  int n_elems = fh_expr_list_size(expr->elem_list);
  int array_reg = alloc_n_regs(c, loc, n_elems + 1);
  if (array_reg < 0)
    return -1;
  int reg = array_reg + 1;
  for (struct fh_p_expr *e = expr->elem_list; e != NULL; e = e->next) {
    if (compile_expr_to_reg(c, e, reg++) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AU(OPC_NEWARRAY, array_reg, n_elems)) < 0)
    return -1;

  for (int i = 1; i < n_elems+1; i++)
    free_reg(c, array_reg+i);

  return array_reg;
}

static int compile_map_lit(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_map_lit *expr)
{
  int n_elems = fh_expr_list_size(expr->elem_list);
  int map_reg = alloc_n_regs(c, loc, n_elems+1);
  if (map_reg < 0)
    return -1;
  int reg = map_reg + 1;
  for (struct fh_p_expr *e = expr->elem_list; e != NULL; e = e->next) {
    if ((reg - map_reg - 1) % 2 == 0 && e->type == EXPR_NULL)
      return fh_compiler_error(c, loc, "map key can't be null");
    if (compile_expr_to_reg(c, e, reg++) < 0)
      return -1;
  }

  if (add_instr(c, loc, MAKE_INSTR_AU(OPC_NEWMAP, map_reg, n_elems)) < 0)
    return -1;

  for (int i = 1; i < n_elems+1; i++)
    free_reg(c, map_reg+i);

  return map_reg;
}

static int compile_inner_func_to_reg(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *expr, int dest_reg)
{
  struct fh_func_def *func_def = new_func_def(c, loc, NULL, expr->n_params);
  if (! func_def)
    return -1;
  if (compile_func(c, loc, expr, func_def) < 0)
    return -1;

  /*
   * TODO: if func_def doesn't reference variables outside its body,
   * we can optimize this by creating the closure now and adding it as
   * a constant instead of writing the following code that creates it.
   */
  
  int k = add_const_func_def(c, loc, func_def);
  if (k >= 0)
    k += MAX_FUNC_REGS + 1;
  if (add_instr(c, loc, MAKE_INSTR_AB(OPC_CLOSURE, dest_reg, k)) < 0)
    return -1;
  
  return dest_reg;
}

static int compile_inner_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *expr)
{
  int dest_reg = alloc_reg(c, loc, TMP_VARIABLE);
  if (dest_reg < 0)
    return -1;
  return compile_inner_func_to_reg(c, loc, expr, dest_reg);
}

static int compile_expr(struct fh_compiler *c, struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_VAR:       return compile_var(c, expr->loc, expr->data.var);
  case EXPR_BIN_OP:    return compile_bin_op(c, expr->loc, &expr->data.bin_op);
  case EXPR_UN_OP:     return compile_un_op(c, expr->loc, &expr->data.un_op);
  case EXPR_FUNC_CALL: return compile_func_call(c, expr->loc, &expr->data.func_call);
  case EXPR_ARRAY_LIT: return compile_array_lit(c, expr->loc, &expr->data.array_lit);
  case EXPR_MAP_LIT:   return compile_map_lit(c, expr->loc, &expr->data.map_lit);
  case EXPR_INDEX:     return compile_index(c, expr->loc, &expr->data.index);
  case EXPR_FUNC:      return compile_inner_func(c, expr->loc, &expr->data.func);

  case EXPR_NULL:
    {
      int k = add_const_null(c, expr->loc);
      if (k >= 0)
        k += MAX_FUNC_REGS + 1;
      return k;
    }
    
  case EXPR_BOOL:
    {
      int k = add_const_bool(c, expr->loc, expr->data.b);
      if (k >= 0)
        k += MAX_FUNC_REGS + 1;
      return k;
    }
    
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
  case EXPR_FUNC:      return compile_inner_func_to_reg(c, expr->loc, &expr->data.func, dest_reg);
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
  free_tmp_regs(c);
  return 0;
}

static int get_opcode_for_test(struct fh_compiler *c, struct fh_src_loc loc, uint32_t op, bool *invert)
{
  switch (op) {
  case '<':        *invert = false; return OPC_CMP_LT;
  case '>':        *invert = true;  return OPC_CMP_LE;
  case AST_OP_LE:  *invert = false; return OPC_CMP_LE;
  case AST_OP_GE:  *invert = true;  return OPC_CMP_LT;
  case AST_OP_EQ:  *invert = false; return OPC_CMP_EQ;
  case AST_OP_NEQ: *invert = true;  return OPC_CMP_EQ;
    
  default:
    return fh_compiler_error(c, loc, "invalid operator for test: '%u", op);
  }
}

static int compile_test_bin_op(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_bin_op *bin_op, bool invert_test)
{
  int left_rk = compile_expr(c, bin_op->left);
  if (left_rk < 0)
    return -1;
  int right_rk = compile_expr(c, bin_op->right);
  if (right_rk < 0)
      return -1;
  bool invert = false;
  int opcode = get_opcode_for_test(c, loc, bin_op->op, &invert);
  if (opcode < 0)
    return -1;
  if (add_instr(c, loc, MAKE_INSTR_ABC(opcode, invert_test ^ invert, left_rk, right_rk)) < 0)
    return -1;
  return 0;
}

static int compile_test(struct fh_compiler *c, struct fh_p_expr *test, bool invert_test)
{
  if (test->type == EXPR_UN_OP && test->data.un_op.op == '!') {
    invert_test = ! invert_test;
    test = test->data.un_op.arg;
  }
  
  if (test->type == EXPR_BIN_OP && is_test_bin_op(&test->data.bin_op))
    return compile_test_bin_op(c, test->loc, &test->data.bin_op, invert_test);

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
  if (compile_test(c, stmt_if->test, false) < 0)
    return -1;
  free_tmp_regs(c);

  // jmp to_false
  int addr_jmp_to_false = get_cur_pc(c);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
    return -1;

  if (compile_stmt(c, stmt_if->true_stmt) < 0)
    return -1;

  // jmp to_end
  int addr_jmp_to_end = get_cur_pc(c);
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
  struct func_info *fi = c->func_info;

  int parent_top_break_addr = (fi->break_addr_list) ? fi->break_addr_list->num : -1;
  
  int addr_jmp_to_end = -1;
  int start_addr = get_cur_pc(c);
  if (! (stmt_while->test->type == EXPR_BOOL && stmt_while->test->data.b)) {
    if (compile_test(c, stmt_while->test, false) < 0)
      return -1;
    free_tmp_regs(c);
    
    // jmp to_end
    addr_jmp_to_end = get_cur_pc(c);
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, 0)) < 0)
      return -1;
  }

  // statement
  switch (stmt_while->stmt->type) {
  case STMT_VAR_DECL: return fh_compiler_error(c, stmt_while->stmt->loc, "variable declaration must be inside block");
  case STMT_BREAK:    return fh_compiler_error(c, stmt_while->stmt->loc, "break must be inside while block");
  case STMT_CONTINUE: return fh_compiler_error(c, stmt_while->stmt->loc, "continue must be inside while block");
    
  case STMT_BLOCK:
    if (compile_block(c, stmt_while->stmt->loc, &stmt_while->stmt->data.block, COMP_BLOCK_WHILE, start_addr) < 0)
      return -1;
    break;

  default:
    if (compile_stmt(c, stmt_while->stmt) < 0)
      return -1;
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, 0, start_addr - get_cur_pc(c) - 1)) < 0)
      return -1;
  }

  // to_end:
  int addr_end = get_cur_pc(c);
  if (addr_jmp_to_end >= 0) {
    if (set_jmp_target(c, loc, addr_jmp_to_end, addr_end) < 0)
      return -1;
  }

  struct break_addr *ba = fi->break_addr_list;
  while (ba != NULL) {
    if (ba->num < parent_top_break_addr)
      break;
    if (set_jmp_target(c, loc, ba->address, addr_end) < 0)
      return -1;
    ba = ba->next;
  }
  fi->break_addr_list = ba;
  return 0;
}

static int compile_break(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct block_info *bi = get_cur_block_info(c, COMP_BLOCK_WHILE);
  if (! bi)
    return fh_compiler_error(c, loc, "break must be inside while");

  int num_open_upvals = get_num_open_upvals(c, loc, bi->parent_num_regs);
  struct break_addr *ba = new_break_address(c, loc);
  if (! ba)
    return fh_compiler_error(c, loc, "out of memory");
  ba->address = get_cur_pc(c);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, num_open_upvals, 0)) < 0)
    return -1;
  return 0;
}

static int compile_continue(struct fh_compiler *c, struct fh_src_loc loc)
{
  struct block_info *bi = get_cur_block_info(c, COMP_BLOCK_WHILE);
  if (! bi)
    return fh_compiler_error(c, loc, "continue must be inside while");

  int num_open_upvals = get_num_open_upvals(c, loc, bi->parent_num_regs);
  if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, num_open_upvals, bi->start_addr - get_cur_pc(c) - 1)) < 0)
    return -1;
  return 0;
}

static int compile_return(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_return *ret)
{
  if (ret->val) {
    int tmp_rk = compile_expr(c, ret->val);
    if (tmp_rk < 0)
      return -1;
    free_tmp_regs(c);
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
    
  case STMT_EXPR:
    if (compile_expr(c, stmt->data.expr) < 0)
      return -1;
    free_tmp_regs(c);
    return 0;

  case STMT_VAR_DECL:  return compile_var_decl(c, stmt->loc, &stmt->data.decl);
  case STMT_BLOCK:     return compile_block(c, stmt->loc, &stmt->data.block, COMP_BLOCK_PLAIN, -1);
  case STMT_RETURN:    return compile_return(c, stmt->loc, &stmt->data.ret);
  case STMT_IF:        return compile_if(c, stmt->loc, &stmt->data.stmt_if);
  case STMT_WHILE:     return compile_while(c, stmt->loc, &stmt->data.stmt_while);
  case STMT_BREAK:     return compile_break(c, stmt->loc);
  case STMT_CONTINUE:  return compile_continue(c, stmt->loc);
  }

  return fh_compiler_error(c, stmt->loc, "invalid statement node type: %d", stmt->type);
}

static int compile_block(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_stmt_block *block, enum compiler_block_type block_type, int32_t block_start_addr)
{
  struct block_info *bi = new_block_info(c);
  if (! bi)
    return fh_compiler_error(c, loc, "out of memory");

  bi->type = block_type;
  bi->start_addr = block_start_addr;
  bi->parent_num_regs = get_top_var_reg(c) + 1;

  for (struct fh_p_stmt *s = block->stmt_list; s != NULL; s = s->next) {
    if (compile_stmt(c, s) < 0)
      return -1;
  }

  int num_open_upvals = get_num_open_upvals(c, loc, bi->parent_num_regs);
  switch (bi->type) {
  case COMP_BLOCK_WHILE:
    if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, num_open_upvals, bi->start_addr - get_cur_pc(c) - 1)) < 0)
      return -1;
    break;

  case COMP_BLOCK_PLAIN:
    if (num_open_upvals > 0) {
      if (add_instr(c, loc, MAKE_INSTR_AS(OPC_JMP, num_open_upvals, 0)) < 0)
        return -1;
    }
    break;

  case COMP_BLOCK_FUNC:
    break;
  }

  free_var_regs(c, bi->parent_num_regs);
  return 0;
}

static int compile_func(struct fh_compiler *c, struct fh_src_loc loc, struct fh_p_expr_func *func, struct fh_func_def *func_def)
{
  struct func_info *fi = new_func_info(c, loc);
  if (! fi)
    return -1;

  for (int i = 0; i < func_def->n_params; i++) {
    if (alloc_reg(c, loc, func->params[i]) < 0)
      goto err;
  }

  if (compile_block(c, loc, &func->body, COMP_BLOCK_FUNC, -1) < 0)
    goto err;

  struct fh_p_stmt *last_stmt;
  for (last_stmt = func->body.stmt_list; last_stmt != NULL; last_stmt = last_stmt->next) {
    if (! last_stmt->next)
      break;
  }
  
  if (! last_stmt || last_stmt->type != STMT_RETURN) {
    if (add_instr(c, loc, MAKE_INSTR_AB(OPC_RET, 0, 0)) < 0)
      goto err;
  }

  if (code_stack_shrink_to_fit(&fi->code) < 0
      || value_stack_shrink_to_fit(&fi->consts) < 0
      || fh_buf_shrink_to_fit(&fi->code_src_loc) < 0) {
    fh_compiler_error(c, loc, "out of memory");
    goto err;
  }
    
  func_def->n_regs = fi->num_regs;
  
  func_def->code_size = code_stack_size(&fi->code);
  func_def->code = code_stack_data(&fi->code);
  code_stack_init(&fi->code);
  
  func_def->n_consts = value_stack_size(&fi->consts);
  func_def->consts = value_stack_data(&fi->consts);
  value_stack_init(&fi->consts);

  func_def->n_upvals = upval_def_stack_size(&fi->upvals);
  func_def->upvals = upval_def_stack_data(&fi->upvals);
  upval_def_stack_init(&fi->upvals);

  func_def->code_src_loc_size = fi->code_src_loc.size;
  func_def->code_src_loc = fi->code_src_loc.p;
  fh_init_buffer(&fi->code_src_loc, NULL);
  
  pop_func_info(c);
  return 0;

 err:
  pop_func_info(c);
  return -1;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func, struct fh_func_def *func_def)
{
  if (compile_func(c, func->loc, &func->func->data.func, func_def) < 0)
    return -1;

  if (c->func_info && c->func_info->parent)
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

  int pin_state = fh_get_pin_state(c->prog);
  
  for (struct fh_p_named_func *f = c->ast->func_list; f != NULL; f = f->next) {
    const char *name = get_func_name(c, f);
    if (! name)
      goto err;
    
    if (fh_get_global_func_by_name(c->prog, name))
      return fh_compiler_error(c, f->loc, "function '%s' already exists", name);

    struct fh_func_def *func_def = new_func_def(c, f->loc, name, f->func->data.func.n_params);
    if (! func_def) {
      fh_compiler_error(c, f->loc, "out of memory");
      goto err;
    }
    
    struct fh_closure *closure = fh_make_closure(c->prog, true, func_def);
    if (! closure) {
      fh_compiler_error(c, f->loc, "out of memory");
      goto err;
    }
    if (fh_add_global_func(c->prog, closure) < 0) {
      fh_compiler_error(c, f->loc, "out of memory");
      goto err;
    }
  }

  for (struct fh_p_named_func *f = c->ast->func_list; f != NULL; f = f->next) {
    const char *name = get_func_name(c, f);
    if (! name)
      goto err;
    struct fh_closure *closure = fh_get_global_func_by_name(c->prog, name);
    if (! closure) {
      fh_compiler_error(c, f->loc, "INTERNAL COMPILER ERROR: can't find function '%s'", name);
      goto err;
    }
    if (compile_named_func(c, f, closure->func_def) < 0)
      goto err;
  }

  fh_restore_pin_state(c->prog, pin_state);
  return 0;

 err:
  fh_restore_pin_state(c->prog, pin_state);
  return -1;
}
