/* ast.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"

static struct {
  char name[4];
  enum fh_op_assoc assoc;
  int32_t prec;
} ast_ops[] = {
  { "=",  FH_ASSOC_RIGHT,  10 },
  
  { "||", FH_ASSOC_LEFT,   20 },
  { "&&", FH_ASSOC_LEFT,   30 },
  
  { "==", FH_ASSOC_LEFT,   40 },
  { "!=", FH_ASSOC_LEFT,   40 },
  { "<",  FH_ASSOC_LEFT,   50 },
  { ">",  FH_ASSOC_LEFT,   50 },
  { "<=", FH_ASSOC_LEFT,   50 },
  { ">=", FH_ASSOC_LEFT,   50 },
  
  { "+",  FH_ASSOC_LEFT,   60 },
  { "-",  FH_ASSOC_LEFT,   60 },
  { "*",  FH_ASSOC_LEFT,   70 },
  { "/",  FH_ASSOC_LEFT,   70 },

  { "-",  FH_ASSOC_PREFIX, 80 },
  { "!",  FH_ASSOC_PREFIX, 80 },

  { "^",  FH_ASSOC_RIGHT,  90 },
  
  { ".",  FH_ASSOC_RIGHT,  FUNC_CALL_PREC+10 },
};

struct fh_ast *fh_new_ast(void)
{
  struct fh_ast *ast = malloc(sizeof(struct fh_ast));
  if (! ast)
    return NULL;
  ast->symtab = NULL;
  fh_init_op_table(&ast->op_table);
  fh_init_buffer(&ast->string_pool);
  fh_init_stack(&ast->funcs, sizeof(struct fh_p_named_func));

  for (int i = 0; i < sizeof(ast_ops)/sizeof(ast_ops[0]); i++) {
    if (fh_add_op(&ast->op_table, ast_ops[i].name, ast_ops[i].prec, ast_ops[i].assoc) < 0)
      goto err;
  }
  
  ast->symtab = fh_symtab_new();
  if (! ast->symtab)
    goto err;
  
  return ast;

 err:
  fh_free_ast(ast);
  return NULL;
}

void fh_free_ast(struct fh_ast *ast)
{
  for (int i = 0; i < ast->funcs.num; i++) {
    struct fh_p_named_func *func = fh_stack_item(&ast->funcs, i);
    fh_free_named_func(*func);
  }
  fh_free_stack(&ast->funcs);

  fh_free_op_table(&ast->op_table);
  fh_free_buffer(&ast->string_pool);
  if (ast->symtab)
    fh_symtab_free(ast->symtab);
  free(ast);
}

const uint8_t *fh_get_ast_symbol(struct fh_ast *ast, fh_symbol_id id)
{
  return fh_symtab_get_symbol(ast->symtab, id);
}

const uint8_t *fh_get_ast_string(struct fh_ast *ast, fh_string_id id)
{
  return ast->string_pool.p + id;
}

const uint8_t *fh_get_ast_op(struct fh_ast *ast, uint32_t op)
{
  static uint8_t name[4];
  memcpy(name, &op, sizeof(name));
  return name;
}

struct fh_p_expr *fh_new_expr(struct fh_parser *p, struct fh_src_loc loc, enum fh_expr_type type)
{
  struct fh_p_expr *expr = malloc(sizeof(struct fh_p_expr));
  if (! expr)
    return fh_parse_error_oom(p, loc);
  expr->type = type;
  expr->loc = loc;
  return expr;
}

struct fh_p_stmt *fh_new_stmt(struct fh_parser *p, struct fh_src_loc loc, enum fh_stmt_type type)
{
  struct fh_p_stmt *stmt = malloc(sizeof(struct fh_p_stmt));
  if (! stmt)
    return fh_parse_error_oom(p, loc);
  stmt->type = type;
  stmt->loc = loc;
  return stmt;
}

void fh_free_func(struct fh_p_expr_func func)
{
  if (func.params)
    free(func.params);
  fh_free_block(func.body);
}

void fh_free_named_func(struct fh_p_named_func func)
{
  fh_free_func(func.func);
}

void fh_free_expr_children(struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_NONE: return;
  case EXPR_VAR: return;
  case EXPR_NUMBER: return;
  case EXPR_STRING: return;

  case EXPR_ASSIGN:
    fh_free_expr(expr->data.assign.dest);
    fh_free_expr(expr->data.assign.val);
    return;

  case EXPR_UN_OP:
    fh_free_expr(expr->data.un_op.arg);
    return;

  case EXPR_BIN_OP:
    fh_free_expr(expr->data.bin_op.left);
    fh_free_expr(expr->data.bin_op.right);
    return;

  case EXPR_FUNC_CALL:
    fh_free_expr(expr->data.func_call.func);
    if (expr->data.func_call.args) {
      for (int i = 0; i < expr->data.func_call.n_args; i++)
        fh_free_expr_children(&expr->data.func_call.args[i]);
      free(expr->data.func_call.args);
    }
    return;

  case EXPR_FUNC:
    fh_free_func(expr->data.func);
    return;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown expression type '%d'\n", expr->type);
}

void fh_free_expr(struct fh_p_expr *expr)
{
  if (expr) {
    fh_free_expr_children(expr);
    free(expr);
  }
}

void fh_free_stmt_children(struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE: return;
  case STMT_EMPTY: return;
  case STMT_BREAK: return;
  case STMT_CONTINUE: return;

  case STMT_EXPR:
    fh_free_expr(stmt->data.expr);
    return;

  case STMT_VAR_DECL:
    fh_free_expr(stmt->data.decl.val);
    return;

  case STMT_BLOCK:
    fh_free_block(stmt->data.block);
    return;

  case STMT_RETURN:
    fh_free_expr(stmt->data.ret.val);
    return;

  case STMT_IF:
    fh_free_expr(stmt->data.stmt_if.test);
    fh_free_stmt(stmt->data.stmt_if.true_stmt);
    fh_free_stmt(stmt->data.stmt_if.false_stmt);
    return;

  case STMT_WHILE:
    fh_free_expr(stmt->data.stmt_while.test);
    fh_free_stmt(stmt->data.stmt_while.stmt);
    return;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown statement type '%d'\n", stmt->type);
}

void fh_free_stmt(struct fh_p_stmt *stmt)
{
  if (stmt) {
    fh_free_stmt_children(stmt);
    free(stmt);
  }
}

void fh_free_stmts(struct fh_p_stmt **stmts, int n_stmts)
{
  if (stmts) {
    for (int i = 0; i < n_stmts; i++)
      fh_free_stmt(stmts[i]);
  }
}

void fh_free_block(struct fh_p_stmt_block block)
{
  if (block.stmts) {
    fh_free_stmts(block.stmts, block.n_stmts);
    free(block.stmts);
  }
}
