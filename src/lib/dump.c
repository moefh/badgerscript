/* dump.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"

#define INDENT 4

struct fh_output {
  FILE *f;
};

static void output(struct fh_output *out, char *fmt, ...) __attribute__((format (printf, 2, 3)));
static void output(struct fh_output *out, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if (out && out->f)
    vfprintf(out->f, fmt, ap);
  else
    vprintf(fmt, ap);
  va_end(ap);
}

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

static void dump_string(struct fh_ast *ast, struct fh_output *out, const uint8_t *str)
{
  output(out, "\"");
  for (const uint8_t *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': output(out, "\\n"); break;
    case '\r': output(out, "\\r"); break;
    case '\t': output(out, "\\t"); break;
    case '\\': output(out, "\\\\"); break;
    case '"': output(out, "\\\""); break;
    default:
      output(out, "%c", *p);
      break;
    }
  }
  output(out, "\"");
}

static void dump_expr(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_NONE:
    output(out, "<INTERNAL ERROR: expression node of type 'NONE'>");
    return;
    
  case EXPR_VAR:
    output(out, "%s", fh_get_ast_symbol(ast, expr->data.var));
    return;

  case EXPR_NUMBER:
    output(out, "%g", expr->data.num);
    return;

  case EXPR_STRING:
    dump_string(ast, out, fh_get_ast_string(ast, expr->data.str));
    return;

  case EXPR_BIN_OP:
    if (expr_needs_paren(expr->data.bin_op.left)) output(out, "(");
    dump_expr(ast, out, indent, expr->data.bin_op.left);
    if (expr_needs_paren(expr->data.bin_op.left)) output(out, ")");
    output(out, " %s ", fh_get_ast_op(ast, expr->data.bin_op.op));
    if (expr_needs_paren(expr->data.bin_op.right)) output(out, "(");
    dump_expr(ast, out, indent, expr->data.bin_op.right);
    if (expr_needs_paren(expr->data.bin_op.right)) output(out, ")");
    return;

  case EXPR_UN_OP:
    output(out, "%s ", fh_get_ast_op(ast, expr->data.un_op.op));
    if (expr_needs_paren(expr->data.un_op.arg)) output(out, "(");
    dump_expr(ast, out, indent, expr->data.un_op.arg);
    if (expr_needs_paren(expr->data.un_op.arg)) output(out, ")");
    return;
    
  case EXPR_FUNC_CALL:
    if (expr_needs_paren(expr->data.func_call.func)) output(out, "(");
    dump_expr(ast, out, indent, expr->data.func_call.func);
    if (expr_needs_paren(expr->data.func_call.func)) output(out, ")");
    output(out, "(");
    for (int i = 0; i < expr->data.func_call.n_args; i++) {
      dump_expr(ast, out, indent, &expr->data.func_call.args[i]);
      if (i+1 < expr->data.func_call.n_args)
        output(out, ", ");
    }
    output(out, ")");
    return;

  case EXPR_ASSIGN:
    dump_expr(ast, out, indent, expr->data.assign.dest);
    output(out, " = ");
    dump_expr(ast, out, indent, expr->data.assign.val);
    return;

  case EXPR_FUNC:
    output(out, "<...func...>");
    return;
  }
  
  output(out, "<unknown expr type: %d>", expr->type);
}

static void dump_block(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt_block block);

static void dump_stmt(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE:
    output(out, "%*s<INTERNAL ERROR: statement node of type 'NONE'>;", indent, "");
    return;

  case STMT_EMPTY:
    output(out, "%*s;\n", indent, "");
    return;

  case STMT_BREAK:
    output(out, "%*sbreak;\n", indent, "");
    return;

  case STMT_CONTINUE:
    output(out, "%*scontinue;\n", indent, "");
    return;

  case STMT_VAR_DECL:
    output(out, "%*svar %s", indent, "", fh_get_ast_symbol(ast, stmt->data.decl.var));
    if (stmt->data.decl.val) {
      output(out, " = ");
      dump_expr(ast, out, indent+INDENT, stmt->data.decl.val);
    }
    output(out, ";\n");
    return;

  case STMT_EXPR:
    output(out, "%*s", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.expr);
    output(out, ";\n");
    return;

  case STMT_RETURN:
    output(out, "%*sreturn", indent, "");
    if (stmt->data.ret.val) {
      output(out, " ");
      dump_expr(ast, out, indent+INDENT, stmt->data.ret.val);
    }
    output(out, ";\n");
    return;

  case STMT_BLOCK:
    output(out, "%*s", indent, "");
    dump_block(ast, out, indent, stmt->data.block);
    output(out, "\n");
    return;

  case STMT_IF:
    output(out, "%*sif (", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.stmt_if.test);
    output(out, ")");

    if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK) {
      output(out, " ");
      dump_block(ast, out, indent, stmt->data.stmt_if.true_stmt->data.block);
    } else {
      output(out, "\n");
      dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_if.true_stmt);
    }

    if (stmt->data.stmt_if.false_stmt) {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        output(out, " else");
      else
        output(out, "%*selse", indent, "");
      if (stmt->data.stmt_if.false_stmt->type == STMT_BLOCK) {
        output(out, " ");
        dump_block(ast, out, indent, stmt->data.stmt_if.false_stmt->data.block);
        output(out, "\n");
      } else {
        output(out, "\n");
        dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_if.false_stmt);
      }
    } else {
      if (stmt->data.stmt_if.true_stmt->type == STMT_BLOCK)
        output(out, "\n");
    }
    return;

  case STMT_WHILE:
    output(out, "%*swhile (", indent, "");
    dump_expr(ast, out, indent+INDENT, stmt->data.stmt_while.test);
    output(out, ")");

    if (stmt->data.stmt_while.stmt->type == STMT_BLOCK) {
      output(out, " ");
      dump_block(ast, out, indent, stmt->data.stmt_while.stmt->data.block);
      output(out, "\n");
    } else {
      output(out, "\n");
      dump_stmt(ast, out, indent+INDENT, stmt->data.stmt_while.stmt);
    }
    return;
  }

  output(out, "%*s# unknown statement type: %d\n", indent, "", stmt->type);
}

static void dump_block(struct fh_ast *ast, struct fh_output *out, int indent, struct fh_p_stmt_block block)
{
  output(out, "{\n");
  for (int i = 0; i < block.n_stmts; i++)
    dump_stmt(ast, out, indent+INDENT, block.stmts[i]);
  output(out, "%*s}", indent, "");
}

void fh_dump_expr(struct fh_ast *ast, struct fh_output *out, struct fh_p_expr *expr)
{
  dump_expr(ast, out, 0, expr);
}

void fh_dump_named_func(struct fh_ast *ast, struct fh_output *out, struct fh_p_named_func *func)
{
  output(out, "function %s(", fh_get_ast_symbol(ast, func->name));
  for (int i = 0; i < func->func.n_params; i++) {
    output(out, "%s", fh_get_ast_symbol(ast, func->func.params[i]));
    if (i+1 < func->func.n_params)
      output(out, ", ");
  }
  output(out, ") ");
  dump_block(ast, out, 0, func->func.body);
  output(out, "\n");
  output(out, "\n");
}
