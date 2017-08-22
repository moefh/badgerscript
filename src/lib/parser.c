/* parser.c */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "parser.h"
#include "ast.h"
#include "parser_stacks.h"

static struct fh_p_stmt_block *parse_block(struct fh_parser *p, struct fh_p_stmt_block *block);
static struct fh_p_expr *parse_func(struct fh_parser *p);
static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars);
static struct fh_p_stmt *parse_stmt(struct fh_parser *p);
static void reset_parser(struct fh_parser *p);

void fh_init_parser(struct fh_parser *p, struct fh_program *prog, struct fh_mem_pool *pool)
{
  p->prog = prog;
  p->pool = pool;
  fh_init_buffer(&p->tmp_buf, pool);
  p->tokenizer = NULL;
  reset_parser(p);
}

void fh_destroy_parser(struct fh_parser *p)
{
  reset_parser(p);
  fh_destroy_buffer(&p->tmp_buf);
}

static void close_tokenizer(struct fh_parser *p)
{
  struct fh_tokenizer *next = p->tokenizer->next;
  fh_close_input(p->tokenizer->in);
  fh_free(p->pool, p->tokenizer);
  p->tokenizer = next;
}

static void reset_parser(struct fh_parser *p)
{
  p->ast = NULL;
  p->tmp_buf.size = 0;
  p->has_saved_tok = 0;
  p->last_loc = fh_make_src_loc(-1,0,0);
  while (p->tokenizer)
    close_tokenizer(p);
}

void *fh_parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  fh_set_error(p->prog, "%s:%d:%d: %s", fh_get_ast_file_name(p->ast, loc.file_id), loc.line, loc.col, str);
  return NULL;
}

void *fh_parse_error_oom(struct fh_parser *p, struct fh_src_loc loc)
{
  return fh_parse_error(p, loc, "out of memory");
}

void *fh_parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected)
{
  return fh_parse_error(p, loc, "expected %s", expected);
}

static int get_token(struct fh_parser *p, struct fh_token *tok)
{
  if (p->has_saved_tok) {
    *tok = p->saved_tok;
    p->has_saved_tok = 0;
    p->last_loc = tok->loc;
    //printf("::: re-token '%s'  @%d:%d\n", fh_dump_token(p->ast, tok), tok->loc.line, tok->loc.col);
    return 0;
  }

  if (! p->tokenizer) {
    tok->type = TOK_EOF;
    tok->loc = p->last_loc;
  }

  while (true) {
    if (fh_read_token(p->tokenizer, tok) < 0)
      return -1;
    if (tok_is_eof(tok)) {
      close_tokenizer(p);
      if (! p->tokenizer)
        break;
      continue;
    }
    break;
  }
  
  p->last_loc = tok->loc;
  //printf(":::::: token '%s'  @%d:%d\n", fh_dump_token(p->ast, tok), tok->loc.line, tok->loc.col);
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

static struct fh_p_expr *new_expr(struct fh_parser *p, struct fh_src_loc loc, enum fh_expr_type type, size_t extra_size)
{
  struct fh_p_expr *expr = fh_new_expr(p->ast, loc, type, extra_size);
  if (! expr)
    return fh_parse_error_oom(p, loc);
  return expr;
}

static struct fh_p_stmt *new_stmt(struct fh_parser *p, struct fh_src_loc loc, enum fh_stmt_type type, size_t extra_size)
{
  struct fh_p_stmt *stmt = fh_new_stmt(p->ast, loc, type, extra_size);
  if (! stmt)
    return fh_parse_error_oom(p, loc);
  return stmt;
}

static int tok_is_op(struct fh_token *tok, const char *op)
{
  if (tok->type != TOK_OP)
    return 0;

  if (! op)
    return 1;
  
  const char *tok_op = fh_get_token_op(tok);
  if (tok_op != NULL && strcmp(op, tok_op) == 0)
    return 1;
  return 0;
}

static int parse_expr_list(struct fh_parser *p, struct fh_p_expr **ret_list)
{
  struct fh_token tok;

  if (get_token(p, &tok) < 0)
    return -1;
  
  struct fh_p_expr *list = NULL;
  struct fh_p_expr **list_tail = &list;
  int num_exprs = 0;

  if (! tok_is_punct(&tok, ')')) {
    unget_token(p, &tok);
    while (1) {
      struct fh_p_expr *e = parse_expr(p, false, ",)");
      if (! e)
        goto err;
      *list_tail = e;
      list_tail = &e->next;
      num_exprs++;
      
      if (get_token(p, &tok) < 0)
        goto err;
      if (tok_is_punct(&tok, ')'))
        break;
      if (tok_is_punct(&tok, ','))
        continue;
      fh_parse_error(p, tok.loc, "expected ',' or ')'");
      goto err;
    }
  }

  *ret_list = list;
  return num_exprs;
  
 err:
  return -1;
}

static int parse_array_literal(struct fh_parser *p, struct fh_p_expr_array_lit *array_lit)
{
  struct fh_token tok;

  if (get_token(p, &tok) < 0)
    return -1;

  array_lit->elem_list = NULL;
  struct fh_p_expr **list_tail = &array_lit->elem_list;
  
  if (! tok_is_punct(&tok, ']')) {
    unget_token(p, &tok);
    while (1) {
      struct fh_p_expr *e = parse_expr(p, false, ",]");
      if (! e)
        goto err;
      *list_tail = e;
      list_tail = &e->next;
      
      if (get_token(p, &tok) < 0)
        goto err;
      if (tok_is_punct(&tok, ']'))
        break;
      if (tok_is_punct(&tok, ',')) {
        if (get_token(p, &tok) < 0)
          goto err;
        if (tok_is_punct(&tok, ']'))
          break;
        unget_token(p, &tok);
        continue;
      }
      fh_parse_error(p, tok.loc, "expected ',' or ']'");
      goto err;
    }
  }

  return 0;
  
 err:
  return -1;
}

static int parse_map_literal(struct fh_parser *p, struct fh_p_expr_map_lit *map_lit)
{
  struct fh_token tok;

  if (get_token(p, &tok) < 0)
    return -1;
  
  map_lit->elem_list = NULL;
  struct fh_p_expr **list_tail = &map_lit->elem_list;

  if (! tok_is_punct(&tok, '}')) {
    unget_token(p, &tok);
    while (1) {
      // key
      struct fh_p_expr *e = parse_expr(p, true, ":");
      if (! e)
        goto err;
      *list_tail = e;
      list_tail = &e->next;

      // value
      e = parse_expr(p, false, ",}");
      if (! e)
        goto err;
      *list_tail = e;
      list_tail = &e->next;

      if (get_token(p, &tok) < 0)
        goto err;
      if (tok_is_punct(&tok, '}'))
        break;
      if (tok_is_punct(&tok, ',')) {
        if (get_token(p, &tok) < 0)
          goto err;
        if (tok_is_punct(&tok, '}'))
          break;
        unget_token(p, &tok);
        continue;
      }
      fh_parse_error(p, tok.loc, "expected ',' or '}'");
      goto err;
    }
  }

  return 0;
  
 err:
  return -1;
}

static void dump_opn_stack(struct fh_parser *p, struct fh_p_expr *opns)
{
  int size = opn_stack_size(&opns);
  printf("**** opn stack has %d elements\n", size);
  int i = 0;
  for (struct fh_p_expr *e = opns; e != NULL; e = e->next) {
    printf("[%d] ", size - i - 1);
    fh_dump_expr(p->ast, e);
    printf("\n");
    i++;
  }
}

static void dump_opr_stack(struct fh_parser *p, struct opr_info *oprs)
{
  UNUSED(p);
  int size = opr_stack_size(&oprs);
  printf("**** opr stack has %d elements\n", size);
  int i = 0;
  for (struct opr_info *o = oprs; o != NULL; o = o->next) {
    printf("[%d] %s\n", size - i - 1, o->op->name);
    i++;
  }
}

static int resolve_expr_stack(struct fh_parser *p, struct fh_src_loc loc, struct fh_p_expr **opns, struct opr_info **oprs, int32_t stop_prec)
{
  while (1) {
    //printf("********** STACKS ********************************\n");
    //dump_opn_stack(p, *opns);
    //dump_opr_stack(p, *oprs);
    //printf("**************************************************\n");
    
    if (opr_stack_size(oprs) == 0)
      return 0;
    
    struct fh_operator *op;
    struct fh_src_loc op_loc;
    opr_stack_top(oprs, &op, &op_loc);
    int32_t op_prec = (op->assoc == FH_ASSOC_RIGHT) ? op->prec-1 : op->prec;
    if (op_prec < stop_prec)
      return 0;
    uint32_t op_id = op->op;
    enum fh_op_assoc op_assoc = op->assoc;
    struct fh_p_expr *expr = new_expr(p, op_loc, EXPR_NONE, 0);
    if (! expr) {
      fh_parse_error_oom(p, loc);
      return -1;
    }
    
    pop_opr(oprs);
    switch (op_assoc) {
    case FH_ASSOC_RIGHT:
    case FH_ASSOC_LEFT:
      if (opn_stack_size(opns) < 2) {
        fh_parse_error(p, loc, "syntax error");
        return -1;
      }
      expr->type = EXPR_BIN_OP;
      expr->data.bin_op.op = op_id;
      expr->data.bin_op.right = pop_opn(opns);
      expr->data.bin_op.left = pop_opn(opns);
      break;

    case FH_ASSOC_PREFIX:
      if (opn_stack_size(opns) < 1) {
        fh_parse_error(p, loc, "syntax error");
        return -1;
      }
      expr->type = EXPR_UN_OP;
      expr->data.un_op.op = op_id;
      expr->data.un_op.arg = pop_opn(opns);
      break;
    }

    if (expr->type == EXPR_NUMBER) {
      fh_parse_error(p, loc, "bad operator assoc: %d", op_assoc);
      return -1;
    }
   
    push_opn(opns, expr);
  }
}

static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars)
{
  struct fh_p_expr *opns;
  struct opr_info *oprs;
  bool expect_opn = true;

  opns = NULL;
  oprs = NULL;

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
        struct fh_p_expr *func = pop_opn(&opns);
        if (! func) {
          fh_parse_error(p, tok.loc, "syntax error (no function on stack!)");
          goto err;
        }

        expr = new_expr(p, func->loc, EXPR_FUNC_CALL, 0);
        if (! expr)
          goto err;
        expr->data.func_call.func = func;
        if (parse_expr_list(p, &expr->data.func_call.arg_list) < 0)
          goto err;
      }

      push_opn(&opns, expr);
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

        //printf("BEFORE:\n"); dump_opn_stack(p, opns); dump_opr_stack(p, oprs);
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, INT32_MIN) < 0)
          goto err;
        //printf("AFTER:\n"); dump_opn_stack(p, opns); dump_opr_stack(p, oprs);

        if (opn_stack_size(&opns) > 1) {
          dump_opn_stack(p, opns);
          dump_opr_stack(p, oprs);
          fh_parse_error(p, tok.loc, "syntax error (stack not empty!)");
          goto err;
        }
        struct fh_p_expr *ret = pop_opn(&opns);
        if (! ret) {
          fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->ast, &tok));
          goto err;
        }
        opr_stack_free(oprs);
        return ret;
      }
    }

    /* . */
    if (tok_is_punct(&tok, '.')) {
      if (expect_opn) {
        fh_parse_error(p, tok.loc, "unexpected '.'");
        goto err;
      }

      if (get_token(p, &tok) < 0)
        goto err;
      if (! tok_is_symbol(&tok)) {
        fh_parse_error(p, tok.loc, "expected name");
        goto err;
      }
      struct fh_p_expr *index = new_expr(p, tok.loc, EXPR_STRING, 0);
      if (! index)
        goto err;
      const char *index_str = fh_get_token_symbol(p->ast, &tok);
      index->data.str = fh_buf_add_string(&p->ast->string_pool, index_str, strlen(index_str));
      if (index->data.str < 0) {
        free(index);
        fh_parse_error_oom(p, tok.loc);
        goto err;
      }

      if (resolve_expr_stack(p, tok.loc, &opns, &oprs, FUNC_CALL_PREC) < 0)
        goto err;
      struct fh_p_expr *container = pop_opn(&opns);
      if (! container) {
        fh_parse_error(p, tok.loc, "syntax error (no container on stack!)");
        goto err;
      }

      struct fh_p_expr *expr = new_expr(p, tok.loc, EXPR_INDEX, 0);
      if (! expr)
        goto err;
      expr->data.index.container = container;
      expr->data.index.index = index;

      push_opn(&opns, expr);
      continue;
    }
    
    /* [ */
    if (tok_is_punct(&tok, '[')) {
      struct fh_p_expr *expr;
      if (expect_opn) {
        expr = new_expr(p, tok.loc, EXPR_ARRAY_LIT, 0);
        if (! expr)
          goto err;
        if (parse_array_literal(p, &expr->data.array_lit) < 0) {
          free(expr);
          goto err;
        }
        expect_opn = false;
      } else {
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, FUNC_CALL_PREC) < 0)
          goto err;
        struct fh_p_expr *container = pop_opn(&opns);
        if (! container) {
          fh_parse_error(p, tok.loc, "syntax error (no container on stack!)");
          goto err;
        }

        expr = new_expr(p, tok.loc, EXPR_INDEX, 0);
        if (! expr)
          goto err;
        expr->data.index.container = container;
        expr->data.index.index = parse_expr(p, true, "]");
        if (! expr->data.index.index)
          goto err;
      }

      push_opn(&opns, expr);
      continue;
    }

    /* { */
    if (tok_is_punct(&tok, '{')) {
      if (! expect_opn) {
        fh_parse_error(p, tok.loc, "unexpected '{'");
        goto err;
      }
      struct fh_p_expr *expr = new_expr(p, tok.loc, EXPR_MAP_LIT, 0);
      if (! expr)
        goto err;
      if (parse_map_literal(p, &expr->data.map_lit) < 0) {
        free(expr);
        goto err;
      }
      expect_opn = false;
      push_opn(&opns, expr);
      continue;
    }
    
    /* operator */
    if (tok_is_op(&tok, NULL)) {
      if (expect_opn) {
        struct fh_operator *op = fh_get_prefix_op(tok.data.op_name);
        if (! op) {
          fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->ast, &tok));
          goto err;
        }
        if (push_opr(&oprs, op, tok.loc) < 0) {
          fh_parse_error_oom(p, tok.loc);
          goto err;
        }
      } else {
        struct fh_operator *op = fh_get_binary_op(tok.data.op_name);
        if (! op) {
          fh_parse_error_expected(p, tok.loc, "'(' or binary operator");
          goto err;
        }
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, op->prec) < 0)
          goto err;
        if (push_opr(&oprs, op, tok.loc) < 0) {
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
      struct fh_p_expr *num = new_expr(p, tok.loc, EXPR_NUMBER, 0);
      if (! num)
        goto err;
      num->data.num = tok.data.num;
      push_opn(&opns, num);
      expect_opn = false;
      continue;
    }

    /* string */
    if (tok_is_string(&tok)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *str = new_expr(p, tok.loc, EXPR_STRING, 0);
      if (! str)
        goto err;
      str->data.str = tok.data.str;
      push_opn(&opns, str);
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
        fh_parse_error(p, tok.loc, "invalid symbol '%s'", fh_dump_token(p->ast, &tok));
        goto err;
      }

      struct fh_p_expr *expr = new_expr(p, tok.loc, EXPR_NONE, 0);
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
      push_opn(&opns, expr);
      expect_opn = false;
      continue;
    }

    /* function */
    if (tok_is_keyword(&tok, KW_FUNCTION)) {
      if (! expect_opn) {
        fh_parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *func = parse_func(p);
      if (! func)
        goto err;
      push_opn(&opns, func);
      expect_opn = false;
      continue;
    }

    /* unrecognized token */
    fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->ast, &tok));
    goto err;
  }

 err:
  opr_stack_free(oprs);
  return NULL;
}

static struct fh_p_stmt *parse_stmt_if(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return fh_parse_error_expected(p, tok.loc, "'('");

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_IF, 0);
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
  return NULL;
}

static struct fh_p_stmt *parse_stmt_while(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return fh_parse_error_expected(p, tok.loc, "'('");

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_WHILE, 0);
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

  struct fh_p_stmt *stmt = new_stmt(p, tok.loc, STMT_NONE, 0);
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
    } else if (tok_is_op(&tok, "=")) {
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

  block->stmt_list = NULL;
  struct fh_p_stmt **list_tail = &block->stmt_list;
  while (1) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (tok_is_punct(&tok, '}'))
      break;
    unget_token(p, &tok);

    struct fh_p_stmt *stmt = parse_stmt(p);
    if (! stmt)
      goto err;
    *list_tail = stmt;
    list_tail = &stmt->next;
  }

  return block;

 err:
  return NULL;
}

static struct fh_p_expr *parse_func(struct fh_parser *p)
{
  struct fh_token tok;

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
  
  struct fh_p_expr *func = new_expr(p, tok.loc, EXPR_FUNC, n_params * sizeof(fh_symbol_id));
  if (! func)
    return NULL;
  func->data.func.n_params = n_params;
  func->data.func.params = (fh_symbol_id *) ((char *)func + sizeof(struct fh_p_expr));
  memcpy(func->data.func.params, params, n_params * sizeof(fh_symbol_id));

  if (! parse_block(p, &func->data.func.body)) {
    free(func);
    return NULL;
  }
  return func;
}

static struct fh_p_named_func *parse_named_func(struct fh_parser *p)
{
  struct fh_token tok;

  // function name
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_symbol(&tok))
    return fh_parse_error_expected(p, tok.loc, "function name");

  struct fh_p_named_func *func = fh_new_named_func(p->ast, tok.loc);
  if (! func)
    return NULL;
  func->name = tok.data.symbol_id;
  
  // rest of function
  func->func = parse_func(p);
  if (! func->func) {
    free(func);
    return NULL;
  }

  return func;
}

static int new_input(struct fh_parser *p, struct fh_src_loc loc, struct fh_input *in)
{
  fh_symbol_id file_id = fh_add_ast_file_name(p->ast, fh_get_input_filename(in));
  if (file_id < 0) {
    fh_parse_error_oom(p, loc);
    return -1;
  }
  
  struct fh_tokenizer *t = fh_malloc(p->pool, sizeof(struct fh_tokenizer));
  if (! t) {
    fh_parse_error_oom(p, loc);
    return -1;
  }
  fh_init_tokenizer(t, p->prog, in, p->ast, &p->tmp_buf, (uint16_t) file_id);
  t->next = p->tokenizer;
  p->tokenizer = t;
  return 0;
}

static int process_include(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return -1;
  if (! tok_is_string(&tok)) {
    fh_parse_error(p, tok.loc, "expected string");
    return -1;
  }

  const char *filename = fh_get_token_string(p->ast, &tok);
  struct fh_input *in = fh_open_input(p->tokenizer->in, filename);
  if (! in) {
    fh_parse_error(p, tok.loc, "can't open file '%s'", filename);
    return -1;
  }
  return new_input(p, tok.loc, in);
}

int fh_parse(struct fh_parser *p, struct fh_ast *ast, struct fh_input *in)
{
  reset_parser(p);
  p->ast = ast;

  if (new_input(p, p->last_loc, in) < 0)
    return -1;
  
  struct fh_p_named_func *func_list = NULL;
  struct fh_p_named_func **func_list_tail = &func_list;

  struct fh_token tok;
  while (1) {
    if (get_token(p, &tok) < 0)
      goto err;

    if (tok_is_eof(&tok))
      break;
    
    if (tok_is_keyword(&tok, KW_INCLUDE)) {
      if (process_include(p) < 0)
        goto err;
      continue;
    }

    if (tok_is_keyword(&tok, KW_FUNCTION)) {
      struct fh_p_named_func *func = parse_named_func(p);
      if (! func)
        goto err;
      *func_list_tail = func;
      func_list_tail = &func->next;
      continue;
    }

    fh_parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->ast, &tok));
    goto err;
  }

  *func_list_tail = p->ast->func_list;
  p->ast->func_list = func_list;
  return 0;

 err:
  return -1;
}
