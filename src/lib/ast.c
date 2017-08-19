/* ast.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"

static struct {
  uint32_t op;
  char name[4];
  enum fh_op_assoc assoc;
  int32_t prec;
} ast_ops[] = {
  { '=',        "=",  FH_ASSOC_RIGHT,  10 },
  
  { AST_OP_OR,  "||", FH_ASSOC_LEFT,   20 },
  { AST_OP_AND, "&&", FH_ASSOC_LEFT,   30 },
  
  { '|',        "|",  FH_ASSOC_LEFT,   40 },
  { '&',        "&",  FH_ASSOC_LEFT,   50 },

  { AST_OP_EQ,  "==", FH_ASSOC_LEFT,   60 },
  { AST_OP_NEQ, "!=", FH_ASSOC_LEFT,   60 },
  { '<',        "<",  FH_ASSOC_LEFT,   70 },
  { '>',        ">",  FH_ASSOC_LEFT,   70 },
  { AST_OP_LE,  "<=", FH_ASSOC_LEFT,   70 },
  { AST_OP_GE,  ">=", FH_ASSOC_LEFT,   70 },

  { '+',        "+",  FH_ASSOC_LEFT,   80 },
  { '-',        "-",  FH_ASSOC_LEFT,   80 },
  { '*',        "*",  FH_ASSOC_LEFT,   90 },
  { '/',        "/",  FH_ASSOC_LEFT,   90 },
  { '%',        "%",  FH_ASSOC_LEFT,   90 },

  { AST_OP_UNM, "-",  FH_ASSOC_PREFIX, 100 },
  { '!',        "!",  FH_ASSOC_PREFIX, 100 },

  { '^',        "^",  FH_ASSOC_RIGHT,  110 },
};

struct fh_ast *fh_new_ast(void)
{
  struct fh_ast *ast = malloc(sizeof(struct fh_ast));
  if (! ast)
    return NULL;
  ast->symtab = NULL;
  fh_init_op_table(&ast->op_table);
  fh_init_buffer(&ast->string_pool);
  named_func_stack_init(&ast->funcs);

  for (int i = 0; i < ARRAY_SIZE(ast_ops); i++) {
    if (fh_add_op(&ast->op_table, ast_ops[i].op, ast_ops[i].name, ast_ops[i].prec, ast_ops[i].assoc) < 0)
      goto err;
  }
  
  ast->symtab = fh_new_symtab();
  if (! ast->symtab)
    goto err;
  
  return ast;

 err:
  fh_free_ast(ast);
  return NULL;
}

void fh_free_ast(struct fh_ast *ast)
{
  stack_foreach(struct fh_p_named_func, *, func, &ast->funcs) {
    fh_free_named_func(*func);
  }
  named_func_stack_free(&ast->funcs);

  fh_free_op_table(&ast->op_table);
  fh_free_buffer(&ast->string_pool);
  if (ast->symtab)
    fh_free_symtab(ast->symtab);
  free(ast);
}

const char *fh_get_ast_symbol(struct fh_ast *ast, fh_symbol_id id)
{
  return fh_get_symbol_name(ast->symtab, id);
}

const char *fh_get_ast_string(struct fh_ast *ast, fh_string_id id)
{
  return ast->string_pool.p + id;
}

const char *fh_get_ast_op(struct fh_ast *ast, uint32_t op)
{
  struct fh_operator *opr = fh_get_op_by_id(&ast->op_table, op);
  if (opr == NULL)
    return NULL;
  return opr->name;
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
  case EXPR_NULL: return;
  case EXPR_BOOL: return;
  case EXPR_NUMBER: return;
  case EXPR_STRING: return;

  case EXPR_UN_OP:
    fh_free_expr(expr->data.un_op.arg);
    return;

  case EXPR_BIN_OP:
    fh_free_expr(expr->data.bin_op.left);
    fh_free_expr(expr->data.bin_op.right);
    return;

  case EXPR_INDEX:
    fh_free_expr(expr->data.index.container);
    fh_free_expr(expr->data.index.index);
    return;

  case EXPR_FUNC_CALL:
    fh_free_expr(expr->data.func_call.func);
    if (expr->data.func_call.args) {
      for (int i = 0; i < expr->data.func_call.n_args; i++)
        fh_free_expr_children(&expr->data.func_call.args[i]);
      free(expr->data.func_call.args);
    }
    return;

  case EXPR_ARRAY_LIT:
    if (expr->data.array_lit.elems) {
      for (int i = 0; i < expr->data.array_lit.n_elems; i++)
        fh_free_expr_children(&expr->data.array_lit.elems[i]);
      free(expr->data.array_lit.elems);
    }
    return;

  case EXPR_MAP_LIT:
    if (expr->data.map_lit.elems) {
      for (int i = 0; i < expr->data.map_lit.n_elems; i++)
        fh_free_expr_children(&expr->data.map_lit.elems[i]);
      free(expr->data.map_lit.elems);
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

int fh_ast_visit_expr_nodes(struct fh_p_expr *expr, int (*visit)(struct fh_p_expr *expr, void *data), void *data)
{
  int ret;

  if ((ret = visit(expr, data)) != 0)
    return ret;
  
  switch (expr->type) {
  case EXPR_NONE: return 0;
  case EXPR_VAR: return 0;
  case EXPR_NULL: return 0;
  case EXPR_BOOL: return 0;
  case EXPR_NUMBER: return 0;
  case EXPR_STRING: return 0;
  case EXPR_FUNC: return 0;

  case EXPR_UN_OP:
    if ((ret = fh_ast_visit_expr_nodes(expr->data.un_op.arg, visit, data)) != 0)
      return ret;
    return 0;

  case EXPR_BIN_OP:
    if ((ret = fh_ast_visit_expr_nodes(expr->data.bin_op.left, visit, data)) != 0)
      return ret;
    if ((ret = fh_ast_visit_expr_nodes(expr->data.bin_op.right, visit, data)) != 0)
      return ret;
    return 0;

  case EXPR_INDEX:
    if ((ret = fh_ast_visit_expr_nodes(expr->data.index.container, visit, data)) != 0)
      return ret;
    if ((ret = fh_ast_visit_expr_nodes(expr->data.index.index, visit, data)) != 0)
      return ret;
    return 0;

  case EXPR_FUNC_CALL:
    if ((ret = fh_ast_visit_expr_nodes(expr->data.func_call.func, visit, data)) != 0)
      return ret;
    if (expr->data.func_call.args) {
      for (int i = 0; i < expr->data.func_call.n_args; i++) {
        if ((ret = fh_ast_visit_expr_nodes(&expr->data.func_call.args[i], visit, data)) != 0)
          return ret;
      }
    }
    return 0;

  case EXPR_ARRAY_LIT:
    if (expr->data.array_lit.elems) {
      for (int i = 0; i < expr->data.array_lit.n_elems; i++) {
        if ((ret = fh_ast_visit_expr_nodes(&expr->data.array_lit.elems[i], visit, data)) != 0)
          return ret;
      }
    }
    return 0;

  case EXPR_MAP_LIT:
    if (expr->data.map_lit.elems) {
      for (int i = 0; i < expr->data.map_lit.n_elems; i++) {
        if ((ret = fh_ast_visit_expr_nodes(&expr->data.map_lit.elems[i], visit, data)) != 0)
          return ret;
      }
    }
    return 0;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown expression type '%d'\n", expr->type);
  return 0;
}
