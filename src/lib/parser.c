/* parser.c */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "parser.h"
#include "ast.h"

static struct fh_p_stmt_block *parse_block(struct fh_parser *p, struct fh_p_stmt_block *block);
static struct fh_p_expr_func *parse_func(struct fh_parser *p, struct fh_p_expr_func *func);
static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars);
static struct fh_p_stmt *parse_stmt(struct fh_parser *p);

static void reset_parser(struct fh_parser *p)
{
  p->ast = NULL;
  p->has_saved_tok = 0;
  p->last_loc = fh_make_src_loc(0,0);
}

void fh_init_parser(struct fh_parser *p, struct fh_program *prog)
{
  p->prog = prog;
  reset_parser(p);
}

void fh_destroy_parser(struct fh_parser *p)
{
  reset_parser(p);
}

void *fh_parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  fh_set_error(p->prog, "%d:%d: %s", loc.line, loc.col, str);
  return NULL;
}

void *fh_parse_error_oom(struct fh_parser *p, struct fh_src_loc loc)
{
  return fh_parse_error(p, loc, "out of memory");
}

void *fh_parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected)
{
  return fh_parse_error(p, loc, "expected '%s'", expected);
}

static int get_token(struct fh_parser *p, struct fh_token *tok)
{
  if (p->has_saved_tok) {
    *tok = p->saved_tok;
    p->has_saved_tok = 0;
    p->last_loc = tok->loc;
    //printf("::: re-token '%s'  @%d:%d\n", fh_dump_token(&p->t, tok), tok->loc.line, tok->loc.col);
    return 0;
  }

  if (fh_read_token(&p->t, tok) < 0)
    return -1;
  p->last_loc = tok->loc;
  //printf(":::::: token '%s'  @%d:%d\n", fh_dump_token(&p->t, tok), tok->loc.line, tok->loc.col);
  return 0;
}

static void unget_token(struct fh_parser *p, struct fh_token *tok)
{
  if (p->has_saved_tok) {
    fprintf(stderr, "ERROR: can't unget token: buffer full\n");
    return;
  }
  p->saved_tok = *tok;
  p->has_saved_tok = 1;
}

static struct fh_p_expr *new_expr(struct fh_parser *p, struct fh_src_loc loc, enum fh_expr_type type)
{
  struct fh_p_expr *expr = malloc(sizeof(struct fh_p_expr));
  if (! expr)
    return fh_parse_error_oom(p, loc);
  expr->type = type;
  expr->loc = loc;
  return expr;
}

static struct fh_p_stmt *new_stmt(struct fh_parser *p, struct fh_src_loc loc, enum fh_stmt_type type)
{
  struct fh_p_stmt *stmt = malloc(sizeof(struct fh_p_stmt));
  if (! stmt)
    return fh_parse_error_oom(p, loc);
  stmt->type = type;
  stmt->loc = loc;
  return stmt;
}

#define tok_is_eof(tok)          ((tok)->type == TOK_EOF)
#define tok_is_number(tok)       ((tok)->type == TOK_NUMBER)
#define tok_is_string(tok)       ((tok)->type == TOK_STRING)
#define tok_is_punct(tok, p)     ((tok)->type == TOK_PUNCT && (tok)->data.punct == (p))
#define tok_is_keyword(tok, kw)  ((tok)->type == TOK_KEYWORD && (tok)->data.keyword == (kw))
#define tok_is_symbol(tok)       ((tok)->type == TOK_SYMBOL)

static int tok_is_op(struct fh_parser *p, struct fh_token *tok, const char *op)
{
  if (tok->type != TOK_OP)
    return 0;

  if (! op)
    return 1;
  
  const char *tok_op = fh_get_token_op(&p->t, tok);
  if (tok_op != NULL && strcmp(op, tok_op) == 0)
    return 1;
  return 0;
}

static int parse_expr_list(struct fh_parser *p, struct fh_p_expr **ret_args, char stop_char)
{
  struct fh_token tok;
  struct expr_stack args;

  expr_stack_init(&args);

  if (get_token(p, &tok) < 0)
    goto err;
  
  char end_expr_chars[3] = { stop_char, ',', '\0' };
  if (! tok_is_punct(&tok, (uint8_t)stop_char)) {
    unget_token(p, &tok);
    while (1) {
      struct fh_p_expr *e = parse_expr(p, false, end_expr_chars);
      if (! e)
        goto err;
      if (! expr_stack_push(&args, e)) {
        fh_free_expr(e);
        goto err;
      }
      free(e);
      
      if (get_token(p, &tok) < 0)
        goto err;
      if (tok_is_punct(&tok, (uint8_t)stop_char))
        break;
      if (tok_is_punct(&tok, ','))
        continue;
      fh_parse_error(p, tok.loc, "expected ',' or '%c'", stop_char);
      goto err;
    }
  }

  expr_stack_shrink_to_fit(&args);
  *ret_args = expr_stack_data(&args);
  return expr_stack_size(&args);
  
 err:
  stack_foreach(struct fh_p_expr, *, e, &args) {
    fh_free_expr_children(e);
  }
  expr_stack_free(&args);
  return -1;
}

static void dump_opn_stack(struct fh_parser *p, struct p_expr_stack *opns)
{
  printf("**** opn stack has %d elements\n", p_expr_stack_size(opns));
  int i = 0;
  stack_foreach(struct fh_p_expr *, *, pe, opns) {
    printf("[%d] ", i++);
    fh_dump_expr(p->ast, *pe);
    printf("\n");
  }
}

static void dump_opr_stack(struct fh_parser *p, struct op_stack *oprs)
{
  UNUSED(p);
  printf("**** opr stack has %d elements\n", op_stack_size(oprs));
  int i = 0;
  stack_foreach(struct fh_operator, *, op, oprs) {
    printf("[%d] %s\n", i++, op->name);
  }
}

static int resolve_expr_stack(struct fh_parser *p, struct fh_src_loc loc, struct p_expr_stack *opns, struct op_stack *oprs, int32_t stop_prec)
{
  while (1) {
    //printf("********** STACKS ********************************\n");
    //dump_opn_stack(p, opns);
    //dump_opr_stack(p, oprs);
    //printf("**************************************************\n");
    
    if (op_stack_size(oprs) == 0)
      return 0;
    
    struct fh_operator *op = op_stack_top(oprs);
    int32_t op_prec = (op->assoc == FH_ASSOC_RIGHT) ? op->prec-1 : op->prec;
    if (op_prec < stop_prec)
      return 0;
    uint32_t op_id = op->op;
    enum fh_op_assoc op_assoc = op->assoc;
    struct fh_p_expr *expr = new_expr(p, loc, EXPR_NONE);
    if (! expr) {
      fh_parse_error_oom(p, loc);
      return -1;
    }
    
    op_stack_pop(oprs, NULL);
    switch (op_assoc) {
    case FH_ASSOC_RIGHT:
    case FH_ASSOC_LEFT:
      if (p_expr_stack_size(opns) < 2) {
        fh_free_expr(expr);
        fh_parse_error(p, loc, "syntax error");
        return -1;
      }
      expr->type = EXPR_BIN_OP;
      expr->data.bin_op.op = op_id;
      p_expr_stack_pop(opns, &expr->data.bin_op.right);
      p_expr_stack_pop(opns, &expr->data.bin_op.left);
      break;

    case FH_ASSOC_PREFIX:
      if (p_expr_stack_size(opns) < 1) {
        fh_free_expr(expr);
        fh_parse_error(p, loc, "syntax error");
        return -1;
      }
      expr->type = EXPR_UN_OP;
      expr->data.bin_op.op = op_id;
      p_expr_stack_pop(opns, &expr->data.un_op.arg);
      break;
    }

    if (expr->type == EXPR_NUMBER) {
      fh_free_expr(expr);
      fh_parse_error(p, loc, "bad operator assoc: %d", op_assoc);
      return -1;
    }
   
    if (! p_expr_stack_push(opns, &expr)) {
      fh_free_expr(expr);
      fh_parse_error_oom(p, loc);
      return -1;
    }
  }
}

static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars)
{
  struct p_expr_stack opns;
  struct op_stack oprs;
  bool expect_opn = true;

  p_expr_stack_init(&opns);
  op_stack_init(&oprs);

  while (1) {
    struct fh_token tok;
    if (get_token(p, &tok) < 0)
      goto err;

    /* ( expr... ) */
    if (tok_is_punct(&tok, '(')) {
      struct fh_p_expr *expr;
      if (expect_opn) {
        expr = parse_expr(p, true, ")");
        if (! expr)
          goto err;
        expect_opn = false;
      } else {
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, FUNC_CALL_PREC) < 0)
          goto err;
        struct fh_p_expr *func;
        if (p_expr_stack_pop(&opns, &func) < 0) {
          fh_parse_error(p, tok.loc, "syntax error (no function on stack!)");
          goto err;
        }

        expr = new_expr(p, tok.loc, EXPR_FUNC_CALL);
        if (! expr) {
          fh_free_expr(func);
          goto err;
        }
        expr->data.func_call.func = func;
        expr->data.func_call.n_args = parse_expr_list(p, &expr->data.func_call.args, ')');
        if (expr->data.func_call.n_args < 0) {
          free(expr);
          fh_free_expr(func);
          goto err;
        }
      }

      if (! p_expr_stack_push(&opns, &expr)) {
        fh_parse_error_oom(p, tok.loc);
        fh_free_expr(expr);
        goto err;
      }
      continue;
    }

    /* stop char */
    if (stop_chars != NULL && tok.type == TOK_PUNCT) {
      bool is_stop = false;
      for (char *stop = stop_chars; *stop != '\0'; stop++) {
        if ((uint8_t)*stop == tok.data.punct) {
          is_stop = true;
          break;
        }
      }
      if (is_stop) {
        if (! consume_stop)
          unget_token(p, &tok);

        //printf("BEFORE:\n"); dump_opn_stack(p, &opns); dump_opr_stack(p, &oprs);
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, INT32_MIN) < 0)
          goto err;
        //printf("AFTER:\n"); dump_opn_stack(p, &opns); dump_opr_stack(p, &oprs);

        if (p_expr_stack_size(&opns) > 1) {
          dump_opn_stack(p, &opns);
          dump_opr_stack(p, &oprs);
          fh_parse_error(p, tok.loc, "syntax error (stack not empty!)");
          goto err;
        }
        struct fh_p_expr *ret;
        if (p_expr_stack_pop(&opns, &ret) < 0) {
          fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(&p->t, &tok));
          goto err;
        }
        p_expr_stack_free(&opns);
        op_stack_free(&oprs);
        return ret;
      }
    }

    /* [ */
    if (tok_is_punct(&tok, '[')) {
      struct fh_p_expr *expr;
      if (expect_opn) {
        expr = new_expr(p, tok.loc, EXPR_ARRAY_LIT);
        if (! expr)
          goto err;
        expr->data.array_lit.n_elems = parse_expr_list(p, &expr->data.array_lit.elems, ']');
        if (expr->data.array_lit.n_elems < 0) {
          free(expr);
          goto err;
        }
        expect_opn = false;
      } else {
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, FUNC_CALL_PREC) < 0)
          goto err;
        struct fh_p_expr *container;
        if (p_expr_stack_pop(&opns, &container) < 0) {
          fh_parse_error(p, tok.loc, "syntax error (no container on stack!)");
          goto err;
        }

        expr = new_expr(p, tok.loc, EXPR_INDEX);
        if (! expr) {
          fh_free_expr(container);
          goto err;
        }
        expr->data.index.container = container;
        expr->data.index.index = parse_expr(p, true, "]");
        if (! expr->data.index.index) {
          free(expr);
          fh_free_expr(container);
          goto err;
        }
      }

      if (! p_expr_stack_push(&opns, &expr)) {
        fh_parse_error_oom(p, tok.loc);
        fh_free_expr(expr);
        goto err;
      }
      continue;
    }
    
    /* operator */
    if (tok_is_op(p, &tok, NULL)) {
      if (expect_opn) {
        struct fh_operator *op = fh_get_prefix_op(&p->ast->op_table, tok.data.op_name);
        if (! op) {
          fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(&p->t, &tok));
          goto err;
        }
        if (! op_stack_push(&oprs, op)) {
          fh_parse_error_oom(p, tok.loc);
          goto err;
        }
      } else {
        struct fh_operator *op = fh_get_binary_op(&p->ast->op_table, tok.data.op_name);
        if (! op) {
          fh_parse_error_expected(p, tok.loc, "'(' or binary operator");
          goto err;
        }
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, op->prec) < 0)
          goto err;
        if (! op_stack_push(&oprs, op)) {
          fh_parse_error_oom(p, tok.loc);
          goto err;
        }
        expect_opn = true;
      }
      continue;
    }
    
    /* number */
    if (tok_is_number(&tok)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *num = new_expr(p, tok.loc, EXPR_NUMBER);
      if (! num)
        goto err;
      num->data.num = tok.data.num;
      if (! p_expr_stack_push(&opns, &num)) {
        fh_free_expr(num);
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* string */
    if (tok_is_string(&tok)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *str = new_expr(p, tok.loc, EXPR_STRING);
      if (! str)
        goto err;
      str->data.str = tok.data.str;
      if (! p_expr_stack_push(&opns, &str)) {
        fh_free_expr(str);
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* symbol */
    if (tok_is_symbol(&tok)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      const char *sym_name = fh_get_ast_symbol(p->ast, tok.data.symbol_id);
      if (! sym_name) {
        fh_parse_error(p, tok.loc, "invalid symbol '%s'", fh_dump_token(&p->t, &tok));
        goto err;
      }

      struct fh_p_expr *expr = new_expr(p, tok.loc, EXPR_NONE);
      if (! expr)
        goto err;
      if (strcmp(sym_name, "null") == 0) {
        expr->type = EXPR_NULL;
      } else if (strcmp(sym_name, "true") == 0) {
        expr->type = EXPR_BOOL;
        expr->data.b = true;
      } else if (strcmp(sym_name, "false") == 0) {
        expr->type = EXPR_BOOL;
        expr->data.b = false;
      } else {
        expr->type = EXPR_VAR;
        expr->data.var = tok.data.symbol_id;
      }
      if (! p_expr_stack_push(&opns, &expr)) {
        fh_free_expr(expr);
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* function */
    if (tok_is_keyword(&tok, KW_FUNCTION)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *func = new_expr(p, tok.loc, EXPR_FUNC);
      if (! func)
        goto err;
      if (! parse_func(p, &func->data.func)) {
        free(func);
        goto err;
      }
      if (! p_expr_stack_push(&opns, &func)) {
        fh_free_expr(func);
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* unrecognized token */
    fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(&p->t, &tok));
    goto err;
  }

 err:
  stack_foreach(struct fh_p_expr *, *, pe, &opns) {
    fh_free_expr(*pe);
  }
  p_expr_stack_free(&opns);
  op_stack_free(&oprs);
  return NULL;
}

static struct fh_p_stmt *parse_stmt_if(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return fh_parse_error_expected(p, tok.loc, "'('");

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_IF);
  if (! stmt)
    return NULL;
  stmt->data.stmt_if.test = NULL;
  stmt->data.stmt_if.true_stmt = NULL;
  stmt->data.stmt_if.false_stmt = NULL;

  stmt->data.stmt_if.test = parse_expr(p, true, ")");
  if (! stmt->data.stmt_if.test)
    goto err;
  
  stmt->data.stmt_if.true_stmt = parse_stmt(p);
  if (! stmt->data.stmt_if.true_stmt)
    goto err;

  if (get_token(p, &tok) < 0)
    goto err;
  if (tok_is_keyword(&tok, KW_ELSE)) {
    stmt->data.stmt_if.false_stmt = parse_stmt(p);
    if (! stmt->data.stmt_if.false_stmt)
      goto err;
  } else {
    unget_token(p, &tok);
    stmt->data.stmt_if.false_stmt = NULL;
  }
  
  return stmt;
  
 err:
  fh_free_stmt(stmt);
  return NULL;
}

static struct fh_p_stmt *parse_stmt_while(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return fh_parse_error_expected(p, tok.loc, "'('");

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_WHILE);
  if (! stmt)
    return NULL;
  stmt->data.stmt_while.test = NULL;
  stmt->data.stmt_while.stmt = NULL;

  stmt->data.stmt_while.test = parse_expr(p, true, ")");
  if (! stmt->data.stmt_while.test)
    goto err;
  
  stmt->data.stmt_while.stmt = parse_stmt(p);
  if (! stmt->data.stmt_while.stmt)
    goto err;

  return stmt;
  
 err:
  fh_free_stmt(stmt);
  return NULL;
}

static struct fh_p_stmt *parse_stmt(struct fh_parser *p)
{
  struct fh_token tok;
  
  if (get_token(p, &tok) < 0)
    return NULL;
  
  // if
  if (tok_is_keyword(&tok, KW_IF)) {
    return parse_stmt_if(p);
  }

  // while
  if (tok_is_keyword(&tok, KW_WHILE)) {
    return parse_stmt_while(p);
  }

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_NONE);
  if (! stmt)
    return NULL;

  // ;
  if (tok_is_punct(&tok, ';')) {
    stmt->type = STMT_EMPTY;
    return stmt;
  }

  // break;
  if (tok_is_keyword(&tok, KW_BREAK)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (! tok_is_punct(&tok, ';'))
      fh_parse_error_expected(p, tok.loc, "';'");
    stmt->type = STMT_BREAK;
    return stmt;
  }

  // continue;
  if (tok_is_keyword(&tok, KW_CONTINUE)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (! tok_is_punct(&tok, ';'))
      fh_parse_error_expected(p, tok.loc, "';'");
    stmt->type = STMT_CONTINUE;
    return stmt;
  }
  
  // var name [= expr] ;
  if (tok_is_keyword(&tok, KW_VAR)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (! tok_is_symbol(&tok))
      return fh_parse_error_expected(p, tok.loc, "variable name");
    stmt->data.decl.var = tok.data.symbol_id;

    if (get_token(p, &tok) < 0)
      goto err;
    if (tok_is_punct(&tok, ';')) {
      stmt->data.decl.val = NULL;
    } else if (tok_is_op(p, &tok, "=")) {
      stmt->data.decl.val = parse_expr(p, true, ";");
      if (! stmt->data.decl.val)
        goto err;
    } else {
      fh_parse_error_expected(p, tok.loc, "'=' or ';'");
      goto err;
    }
    stmt->type = STMT_VAR_DECL;
    return stmt;
  }

  // { ... }
  if (tok_is_punct(&tok, '{')) {
    unget_token(p, &tok);
    if (! parse_block(p, &stmt->data.block))
      goto err;
    stmt->type = STMT_BLOCK;
    return stmt;
  }

  // return [expr] ;
  if (tok_is_keyword(&tok, KW_RETURN)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (tok_is_punct(&tok, ';')) {
      stmt->data.ret.val = NULL;
    } else {
      unget_token(p, &tok);
      stmt->data.ret.val = parse_expr(p, true, ";");
      if (! stmt->data.ret.val)
        goto err;
    }
    stmt->type = STMT_RETURN;
    return stmt;
  }
  
  // expr ;
  unget_token(p, &tok);
  stmt->data.expr = parse_expr(p, true, ";");
  if (! stmt->data.expr)
    goto err;
  stmt->type = STMT_EXPR;
  return stmt;

 err:
  free(stmt);
  return NULL;
}

static struct fh_p_stmt_block *parse_block(struct fh_parser *p, struct fh_p_stmt_block *block)
{
  struct fh_token tok;

  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '{'))
    return fh_parse_error_expected(p, tok.loc, "'{'");

  struct p_stmt_stack stmts;
  p_stmt_stack_init(&stmts);
  while (1) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (tok_is_punct(&tok, '}'))
      break;
    unget_token(p, &tok);

    struct fh_p_stmt *stmt = parse_stmt(p);
    if (! stmt)
      goto err;
    if (! p_stmt_stack_push(&stmts, &stmt)) {
      fh_parse_error_oom(p, tok.loc);
      goto err;
    }
  }

  if (p_stmt_stack_shrink_to_fit(&stmts) < 0)
    goto err;
  block->n_stmts = p_stmt_stack_size(&stmts);
  block->stmts = p_stmt_stack_data(&stmts);
  return block;

 err:
  fh_free_stmts(p_stmt_stack_data(&stmts), p_stmt_stack_size(&stmts));
  p_stmt_stack_free(&stmts);
  return NULL;
}

static struct fh_p_expr_func *parse_func(struct fh_parser *p, struct fh_p_expr_func *func)
{
  struct fh_token tok;

  // param list
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return fh_parse_error_expected(p, tok.loc, "'('");
  if (get_token(p, &tok) < 0)
    return NULL;
  fh_symbol_id params[64];
  int n_params = 0;
  if (! tok_is_punct(&tok, ')')) {
    while (1) {
      if (! tok_is_symbol(&tok))
        return fh_parse_error_expected(p, tok.loc, "name");
      if (n_params >= ARRAY_SIZE(params))
        return fh_parse_error(p, tok.loc, "too many parameters");
      params[n_params++] = tok.data.symbol_id;
      
      if (get_token(p, &tok) < 0)
        return NULL;
      if (tok_is_punct(&tok, ')'))
        break;
      if (! tok_is_punct(&tok, ','))
        return fh_parse_error_expected(p, tok.loc, "')' or ','");
      if (get_token(p, &tok) < 0)
        return NULL;
    }
  }
  
  // function body
  if (! parse_block(p, &func->body))
    return NULL;

  // copy params
  func->n_params = n_params;
  func->params = malloc(n_params * sizeof(params[0]));
  if (! func->params) {
    fh_free_block(func->body);
    return fh_parse_error_oom(p, tok.loc);
  }
  memcpy(&func->params[0], &params[0], n_params * sizeof(params[0]));
  return func;
}

static struct fh_p_named_func *parse_named_func(struct fh_parser *p, struct fh_p_named_func *func)
{
  struct fh_token tok;

  // function name
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_symbol(&tok))
    return fh_parse_error_expected(p, tok.loc, "function name");
  func->loc = tok.loc;
  func->name = tok.data.symbol_id;

  // rest of function
  if (! parse_func(p, &func->func))
    return NULL;

  return func;
}

int fh_parse(struct fh_parser *p, struct fh_ast *ast, struct fh_input *in)
{
  reset_parser(p);
  fh_init_tokenizer(&p->t, p->prog, in, ast);
  
  p->ast = ast;

  struct named_func_stack funcs;
  named_func_stack_init(&funcs);

  struct fh_token tok;
  while (1) {
    if (get_token(p, &tok) < 0)
      goto err;

    if (tok_is_eof(&tok))
      break;
    
    if (tok_is_keyword(&tok, KW_FUNCTION)) {
      struct fh_p_named_func func;
      if (! parse_named_func(p, &func))
        goto err;
      if (! named_func_stack_push(&funcs, &func)) {
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }
      continue;
    }

    fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(&p->t, &tok));
    goto err;
  }
  fh_destroy_tokenizer(&p->t);
  stack_foreach(struct fh_p_named_func, *, f, &funcs) {
    named_func_stack_push(&p->ast->funcs, f);
  }
  named_func_stack_free(&funcs);
  return 0;

 err:
  fh_destroy_tokenizer(&p->t);
  stack_foreach(struct fh_p_named_func, *, f, &funcs) {
    fh_free_named_func(*f);
  }
  named_func_stack_free(&funcs);
  return -1;
}
