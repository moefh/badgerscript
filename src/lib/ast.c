/* ast.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"

struct fh_ast *fh_new_ast(struct fh_mem_pool *pool, struct fh_symtab *file_names)
{
  struct fh_ast *ast = fh_malloc(pool, sizeof(struct fh_ast));
  if (! ast)
    return NULL;
  ast->pool = pool;
  ast->func_list = NULL;
  ast->file_names = file_names;
  fh_init_symtab(&ast->symtab, pool);
  fh_init_buffer(&ast->string_pool, pool);
  return ast;
}

void fh_free_ast(struct fh_ast *ast)
{
  fh_destroy_buffer(&ast->string_pool);
  fh_destroy_symtab(&ast->symtab);
  fh_free(ast->pool, ast);
}

const char *fh_get_ast_symbol(struct fh_ast *ast, fh_symbol_id id)
{
  return fh_get_symbol_name(&ast->symtab, id);
}

const char *fh_get_ast_string(struct fh_ast *ast, fh_string_id id)
{
  return ast->string_pool.p + id;
}

fh_symbol_id fh_add_ast_file_name(struct fh_ast *ast, const char *filename)
{
  return fh_add_symbol(ast->file_names, filename);
}

const char *fh_get_ast_file_name(struct fh_ast *ast, fh_symbol_id file_id)
{
  return fh_get_symbol_name(ast->file_names, file_id);
}

/* node creation */

struct fh_p_named_func *fh_new_named_func(struct fh_ast *ast, struct fh_src_loc loc)
{
  struct fh_p_named_func *func = fh_malloc(ast->pool, sizeof(struct fh_p_named_func));
  func->next = NULL;
  func->loc = loc;
  return func;
}

struct fh_p_expr *fh_new_expr(struct fh_ast *ast, struct fh_src_loc loc, enum fh_expr_type type, size_t extra_size)
{
  struct fh_p_expr *expr = fh_malloc(ast->pool, sizeof(struct fh_p_expr) + extra_size);
  if (! expr)
    return NULL;
  expr->next = NULL;
  expr->type = type;
  expr->loc = loc;
  return expr;
}

struct fh_p_stmt *fh_new_stmt(struct fh_ast *ast, struct fh_src_loc loc, enum fh_stmt_type type, size_t extra_size)
{
  UNUSED(ast);
  struct fh_p_stmt *stmt = fh_malloc(ast->pool, sizeof(struct fh_p_stmt) + extra_size);
  if (! stmt)
    return NULL;
  stmt->next = NULL;
  stmt->type = type;
  stmt->loc = loc;
  return stmt;
}

/* node utility functions */
int fh_expr_list_size(struct fh_p_expr *list)
{
  int n = 0;
  for (struct fh_p_expr *e = list; e != NULL; e = e->next)
    n++;
  return n;
}

int fh_stmt_list_size(struct fh_p_stmt *list)
{
  int n = 0;
  for (struct fh_p_stmt *s = list; s != NULL; s = s->next)
    n++;
  return n;
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
    for (struct fh_p_expr *e = expr->data.func_call.arg_list; e != NULL; e = e->next) {
      if ((ret = fh_ast_visit_expr_nodes(e, visit, data)) != 0)
        return ret;
    }
    return 0;

  case EXPR_ARRAY_LIT:
    for (struct fh_p_expr *e = expr->data.array_lit.elem_list; e != NULL; e = e->next) {
      if ((ret = fh_ast_visit_expr_nodes(e, visit, data)) != 0)
        return ret;
    }
    return 0;

  case EXPR_MAP_LIT:
    for (struct fh_p_expr *e = expr->data.map_lit.elem_list; e != NULL; e = e->next) {
      if ((ret = fh_ast_visit_expr_nodes(e, visit, data)) != 0)
        return ret;
    }
    return 0;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown expression type '%d'\n", expr->type);
  return 0;
}
