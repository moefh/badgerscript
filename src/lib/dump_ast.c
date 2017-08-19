/* dump_ast.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_internal.h"
#include "ast.h"

#define INDENT 4

static bool expr_needs_paren(struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_VAR:
  case EXPR_NUMBER:
  case EXPR_STRING:
  case EXPR_FUNC_CALL:
    return false;

  default:
    return true;
  }
}

static void dump_string(struct fh_ast *ast, const char *str)
{
  UNUSED(ast);
  printf("\"");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': printf("\\n"); break;
    case '\r': printf("\\r"); break;
    case '\t': printf("\\t"); break;
    case '\\': printf("\\\\"); break;
    case '"': printf("\\\""); break;
    default:
      if (*p < 32)
        printf("\\x%02x", (unsigned char) *p);
      else
        printf("%c", *p);
      break;
    }
  }
  printf("\"");
}

static void dump_expr(struct fh_ast *ast, int indent, struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_NONE:
    printf("<INTERNAL ERROR: expression node of type 'NONE'>");
    return;
    
  case EXPR_VAR:
    printf("%s", fh_get_ast_symbol(ast, expr->data.var));
    return;

  case EXPR_NULL:
    printf("null");
    return;

  case EXPR_BOOL:
    printf("%s", (expr->data.b) ? "true" : "false");
    return;

  case EXPR_NUMBER:
    printf("%g", expr->data.num);
    return;

  case EXPR_STRING:
    dump_string(ast, fh_get_ast_string(ast, expr->data.str));
    return;

  case EXPR_BIN_OP:
    if (expr_needs_paren(expr->data.bin_op.left)) printf("(");
    dump_expr(ast, indent, expr->data.bin_op.left);
    if (expr_needs_paren(expr->data.bin_op.left)) printf(")");
    printf(" %s ", fh_get_ast_op(ast, expr->data.bin_op.op));
    if (expr_needs_paren(expr->data.bin_op.right)) printf("(");
    dump_expr(ast, indent, expr->data.bin_op.right);
    if (expr_needs_paren(expr->data.bin_op.right)) printf(")");
    return;

  case EXPR_INDEX:
    if (expr_needs_paren(expr->data.index.container)) printf("(");
    dump_expr(ast, indent, expr->data.index.container);
    if (expr_needs_paren(expr->data.index.container)) printf(")");
    printf("[");
    dump_expr(ast, indent, expr->data.index.index);
    printf("]");
    return;
    
  case EXPR_UN_OP:
    printf("%s", fh_get_ast_op(ast, expr->data.un_op.op));
    if (expr_needs_paren(expr->data.un_op.arg)) printf("(");
    dump_expr(ast, indent, expr->data.un_op.arg);
    if (expr_needs_paren(expr->data.un_op.arg)) printf(")");
    return;
    
  case EXPR_FUNC_CALL:
    if (expr_needs_paren(expr->data.func_call.func)) printf("(");
    dump_expr(ast, indent, expr->data.func_call.func);
    if (expr_needs_paren(expr->data.func_call.func)) printf(")");
    printf("(");
    for (int i = 0; i < expr->data.func_call.n_args; i++) {
      dump_expr(ast, indent, &expr->data.func_call.args[i]);
      if (i+1 < expr->data.func_call.n_args)
        printf(", ");
    }
    printf(")");
    return;

  case EXPR_ARRAY_LIT:
    printf("[ ");
    for (int i = 0; i < expr->data.array_lit.n_elems; i++) {
      dump_expr(ast, indent, &expr->data.array_lit.elems[i]);
      if (i+1 < expr->data.array_lit.n_elems)
        printf(", ");
    }
    printf(" ]");
    return;

  case EXPR_MAP_LIT:
    printf("{ ");
    for (int i = 0; i < expr->data.array_lit.n_elems/2; i++) {
      dump_expr(ast, indent, &expr->data.array_lit.elems[2*i]);
      printf(" : ");
      dump_expr(ast, indent, &expr->data.array_lit.elems[2*i+1]);
      if (2*(i+1)+1 < expr->data.array_lit.n_elems)
        printf(", ");
    }
    if (expr->data.array_lit.n_elems > 0)
      printf(" ");
    printf("}");
    return;

  case EXPR_FUNC:
    printf("<...func...>");
    return;
  }
  
  printf("<unknown expr type: %d>", expr->type);
}

static void dump_block(struct fh_ast *ast, int indent, struct fh_p_stmt_block block);

static void dump_stmt(struct fh_ast *ast, int indent, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
    printf("%*s<INTERNAL ERROR: statement node of type 'NONE'>;", indent, "");
    return;

  case STMT_EMPTY:
    printf("%*s;\n", indent, "");
    return;

  case STMT_BREAK:
    printf("%*sbreak;\n", indent, "");
    return;

  case STMT_CONTINUE:
    printf("%*scontinue;\n", indent, "");
    return;

  case STMT_VAR_DECL:
    printf("%*svar %s", indent, "", fh_get_ast_symbol(ast, stmt->data.decl.var));
    if (stmt->data.decl.val) {
      printf(" = ");
      dump_expr(ast, indent+INDENT, stmt->data.decl.val);
    }
    printf(";\n");
    return;

  case STMT_EXPR:
    printf("%*s", indent, "");
    dump_expr(ast, indent+INDENT, stmt->data.expr);
    printf(";\n");
    return;

  case STMT_RETURN:
    printf("%*sreturn", indent, "");
    if (stmt->data.ret.val) {
      printf(" ");
      dump_expr(ast, indent+INDENT, stmt->data.ret.val);
    }
    printf(";\n");
    return;

  case STMT_BLOCK:
    printf("%*s", indent, "");
    dump_block(ast, indent, stmt->data.block);
    printf("\n");
    return;

  case STMT_IF:
    printf("%*sif (", indent, "");
    dump_expr(ast, indent+INDENT, stmt->data.stmt_if.test);
    printf(")");

    if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK) {
      printf(" ");
      dump_block(ast, indent, stmt->data.stmt_if.true_stmt->data.block);
    } else {
      printf("\n");
      dump_stmt(ast, indent+INDENT, stmt->data.stmt_if.true_stmt);
    }

    if (stmt->data.stmt_if.false_stmt) {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        printf(" else");
      else
        printf("%*selse", indent, "");
      if (stmt->data.stmt_if.false_stmt->type == STMT_BLOCK) {
        printf(" ");
        dump_block(ast, indent, stmt->data.stmt_if.false_stmt->data.block);
        printf("\n");
      } else {
        printf("\n");
        dump_stmt(ast, indent+INDENT, stmt->data.stmt_if.false_stmt);
      }
    } else {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        printf("\n");
    }
    return;

  case STMT_WHILE:
    printf("%*swhile (", indent, "");
    dump_expr(ast, indent+INDENT, stmt->data.stmt_while.test);
    printf(")");

    if (stmt->data.stmt_while.stmt->type == STMT_BLOCK) {
      printf(" ");
      dump_block(ast, indent, stmt->data.stmt_while.stmt->data.block);
      printf("\n");
    } else {
      printf("\n");
      dump_stmt(ast, indent+INDENT, stmt->data.stmt_while.stmt);
    }
    return;
  }

  printf("%*s# unknown statement type: %d\n", indent, "", stmt->type);
}

static void dump_block(struct fh_ast *ast, int indent, struct fh_p_stmt_block block)
{
  printf("{\n");
  for (int i = 0; i < block.n_stmts; i++)
    dump_stmt(ast, indent+INDENT, block.stmts[i]);
  printf("%*s}", indent, "");
}

void fh_dump_expr(struct fh_ast *ast, struct fh_p_expr *expr)
{
  dump_expr(ast, 0, expr);
}

void fh_dump_block(struct fh_ast *ast, struct fh_p_stmt_block block)
{
  dump_block(ast, 0, block);
}

void fh_dump_named_func(struct fh_ast *ast, struct fh_p_named_func *func)
{
  printf("function %s(", fh_get_ast_symbol(ast, func->name));
  for (int i = 0; i < func->func.n_params; i++) {
    printf("%s", fh_get_ast_symbol(ast, func->func.params[i]));
    if (i+1 < func->func.n_params)
      printf(", ");
  }
  printf(") ");
  dump_block(ast, 0, func->func.body);
  printf("\n");
}

void fh_dump_ast(struct fh_ast *ast)
{
  stack_foreach(struct fh_p_named_func, *, f, &ast->funcs) {
    fh_dump_named_func(ast, f);
  }
}
