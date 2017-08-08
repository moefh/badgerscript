/* dump_ast.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
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

static void dump_string(struct fh_ast *ast, struct fh_output *out, const char *str)
{
  UNUSED(ast);
  fh_output(out, "\"");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': fh_output(out, "\\n"); break;
    case '\r': fh_output(out, "\\r"); break;
    case '\t': fh_output(out, "\\t"); break;
    case '\\': fh_output(out, "\\\\"); break;
    case '"': fh_output(out, "\\\""); break;
    default:
      if (*p < 32)
        fh_output(out, "\\x%02x", (unsigned char) *p);
      else
        fh_output(out, "%c", *p);
      break;
    }
  }
  fh_output(out, "\"");
}

static void dump_expr(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_NONE:
    fh_output(out, "<INTERNAL ERROR: expression node of type 'NONE'>");
    return;
    
  case EXPR_VAR:
    fh_output(out, "%s", fh_get_ast_symbol(ast, expr->data.var));
    return;

  case EXPR_NUMBER:
    fh_output(out, "%g", expr->data.num);
    return;

  case EXPR_STRING:
    dump_string(ast, out, fh_get_ast_string(ast, expr->data.str));
    return;

  case EXPR_BIN_OP:
    if (expr_needs_paren(expr->data.bin_op.left)) fh_output(out, "(");
    dump_expr(ast, out, indent, expr->data.bin_op.left);
    if (expr_needs_paren(expr->data.bin_op.left)) fh_output(out, ")");
    fh_output(out, " %s ", fh_get_ast_op(ast, expr->data.bin_op.op));
    if (expr_needs_paren(expr->data.bin_op.right)) fh_output(out, "(");
    dump_expr(ast, out, indent, expr->data.bin_op.right);
    if (expr_needs_paren(expr->data.bin_op.right)) fh_output(out, ")");
    return;

  case EXPR_UN_OP:
    fh_output(out, "%s", fh_get_ast_op(ast, expr->data.un_op.op));
    if (expr_needs_paren(expr->data.un_op.arg)) fh_output(out, "(");
    dump_expr(ast, out, indent, expr->data.un_op.arg);
    if (expr_needs_paren(expr->data.un_op.arg)) fh_output(out, ")");
    return;
    
  case EXPR_FUNC_CALL:
    if (expr_needs_paren(expr->data.func_call.func)) fh_output(out, "(");
    dump_expr(ast, out, indent, expr->data.func_call.func);
    if (expr_needs_paren(expr->data.func_call.func)) fh_output(out, ")");
    fh_output(out, "(");
    for (int i = 0; i < expr->data.func_call.n_args; i++) {
      dump_expr(ast, out, indent, &expr->data.func_call.args[i]);
      if (i+1 < expr->data.func_call.n_args)
        fh_output(out, ", ");
    }
    fh_output(out, ")");
    return;

  case EXPR_FUNC:
    fh_output(out, "<...func...>");
    return;
  }
  
  fh_output(out, "<unknown expr type: %d>", expr->type);
}

static void dump_block(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt_block block);

static void dump_stmt(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
    fh_output(out, "%*s<INTERNAL ERROR: statement node of type 'NONE'>;", indent, "");
    return;

  case STMT_EMPTY:
    fh_output(out, "%*s;\n", indent, "");
    return;

  case STMT_BREAK:
    fh_output(out, "%*sbreak;\n", indent, "");
    return;

  case STMT_CONTINUE:
    fh_output(out, "%*scontinue;\n", indent, "");
    return;

  case STMT_VAR_DECL:
    fh_output(out, "%*svar %s", indent, "", fh_get_ast_symbol(ast, stmt->data.decl.var));
    if (stmt->data.decl.val) {
      fh_output(out, " = ");
      dump_expr(ast, out, indent+INDENT, stmt->data.decl.val);
    }
    fh_output(out, ";\n");
    return;

  case STMT_EXPR:
    fh_output(out, "%*s", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.expr);
    fh_output(out, ";\n");
    return;

  case STMT_RETURN:
    fh_output(out, "%*sreturn", indent, "");
    if (stmt->data.ret.val) {
      fh_output(out, " ");
      dump_expr(ast, out, indent+INDENT, stmt->data.ret.val);
    }
    fh_output(out, ";\n");
    return;

  case STMT_BLOCK:
    fh_output(out, "%*s", indent, "");
    dump_block(ast, out, indent, stmt->data.block);
    fh_output(out, "\n");
    return;

  case STMT_IF:
    fh_output(out, "%*sif (", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.stmt_if.test);
    fh_output(out, ")");

    if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK) {
      fh_output(out, " ");
      dump_block(ast, out, indent, stmt->data.stmt_if.true_stmt->data.block);
    } else {
      fh_output(out, "\n");
      dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_if.true_stmt);
    }

    if (stmt->data.stmt_if.false_stmt) {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        fh_output(out, " else");
      else
        fh_output(out, "%*selse", indent, "");
      if (stmt->data.stmt_if.false_stmt->type == STMT_BLOCK) {
        fh_output(out, " ");
        dump_block(ast, out, indent, stmt->data.stmt_if.false_stmt->data.block);
        fh_output(out, "\n");
      } else {
        fh_output(out, "\n");
        dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_if.false_stmt);
      }
    } else {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        fh_output(out, "\n");
    }
    return;

  case STMT_WHILE:
    fh_output(out, "%*swhile (", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.stmt_while.test);
    fh_output(out, ")");

    if (stmt->data.stmt_while.stmt->type == STMT_BLOCK) {
      fh_output(out, " ");
      dump_block(ast, out, indent, stmt->data.stmt_while.stmt->data.block);
      fh_output(out, "\n");
    } else {
      fh_output(out, "\n");
      dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_while.stmt);
    }
    return;
  }

  fh_output(out, "%*s# unknown statement type: %d\n", indent, "", stmt->type);
}

static void dump_block(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt_block block)
{
  fh_output(out, "{\n");
  for (int i = 0; i < block.n_stmts; i++)
    dump_stmt(ast, out, indent+INDENT, block.stmts[i]);
  fh_output(out, "%*s}", indent, "");
}

void fh_dump_expr(struct fh_ast *ast, struct fh_output *out, struct fh_p_expr *expr)
{
  dump_expr(ast, out, 0, expr);
}

void fh_dump_block(struct fh_ast *ast, struct fh_p_stmt_block block) { dump_block(ast, NULL, 0, block); }

void fh_dump_named_func(struct fh_ast *ast, struct fh_output *out, struct fh_p_named_func *func)
{
  fh_output(out, "function %s(", fh_get_ast_symbol(ast, func->name));
  for (int i = 0; i < func->func.n_params; i++) {
    fh_output(out, "%s", fh_get_ast_symbol(ast, func->func.params[i]));
    if (i+1 < func->func.n_params)
      fh_output(out, ", ");
  }
  fh_output(out, ") ");
  dump_block(ast, out, 0, func->func.body);
  fh_output(out, "\n");
}

void fh_dump_ast(struct fh_ast *ast)
{
  stack_foreach(struct fh_p_named_func *, f, &ast->funcs) {
    fh_dump_named_func(ast, NULL, f);
  }
}
