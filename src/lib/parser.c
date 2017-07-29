/* parser.c */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "fh_i.h"
#include "ast.h"

struct fh_parser {
  struct fh_tokenizer *t;
  struct fh_ast ast;
  uint8_t last_err_msg[256];
  struct fh_src_loc last_loc;
  int has_saved_tok;
  struct fh_token saved_tok;
};

#define FUNC_CALL_PREC 1000

static struct {
  char name[4];
  enum fh_op_assoc assoc;
  int32_t prec;
} ops[] = {
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

static void *parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
static struct fh_p_stmt_block *parse_block(struct fh_parser *p, struct fh_p_stmt_block *block);
static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars);
static struct fh_p_stmt *parse_stmt(struct fh_parser *p);
static void free_named_func(struct fh_p_named_func func);
static void free_func(struct fh_p_expr_func func);
static void free_block(struct fh_p_stmt_block block);
static void free_stmts(struct fh_p_stmt **stmts, int n_stmts);
static void free_stmt(struct fh_p_stmt *stmt);
static void free_expr(struct fh_p_expr *expr);

/* ======================================== */
/* === AST ================================ */

static int init_ast(struct fh_ast *ast)
{
  ast->symtab = fh_symtab_new();
  if (! ast->symtab)
    return -1;
  fh_init_op_table(&ast->op_table);
  fh_init_buffer(&ast->string_pool);
  fh_init_stack(&ast->funcs, sizeof(struct fh_p_named_func));
  return 0;
}

static void free_ast(struct fh_ast *ast)
{
  for (int i = 0; i < ast->funcs.num; i++) {
    struct fh_p_named_func *func = fh_stack_item(&ast->funcs, i);
    free_named_func(*func);
  }
  fh_free_stack(&ast->funcs);

  fh_free_op_table(&ast->op_table);
  fh_free_buffer(&ast->string_pool);
  if (ast->symtab)
    fh_symtab_free(ast->symtab);
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

/* ======================================== */
/* === parser ============================= */

struct fh_parser *fh_new_parser(struct fh_input *in)
{
  struct fh_parser *p = malloc(sizeof(struct fh_parser));
  if (p == NULL)
    return NULL;
  if (init_ast(&p->ast) < 0) {
    free(p);
    return NULL;
  }
  p->has_saved_tok = 0;
  p->last_err_msg[0] = '\0';
  p->last_loc = fh_make_src_loc(0,0);
  p->t = NULL;

  for (int i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
    if (fh_add_op(&p->ast.op_table, ops[i].name, ops[i].prec, ops[i].assoc) < 0)
      goto err;
  }

  p->t = fh_new_tokenizer(in, &p->ast);
  if (! p->t)
    goto err;

  return p;

 err:
  fh_free_parser(p);
  return NULL;
}

void fh_free_parser(struct fh_parser *p)
{
  if (p->t)
    fh_free_tokenizer(p->t);

  free_ast(&p->ast);
  free(p);
}

static void *parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  snprintf((char *) p->last_err_msg, sizeof(p->last_err_msg), "%d:%d: %s", loc.line, loc.col, str);
  return NULL;
}

static void *parse_error_oom(struct fh_parser *p, struct fh_src_loc loc)
{
  return parse_error(p, loc, "out of memory");
}

static void *parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected)
{
  return parse_error(p, loc, "expected '%s'", expected);
}

const uint8_t *fh_get_parser_error(struct fh_parser *p)
{
  return p->last_err_msg;
}

static int get_token(struct fh_parser *p, struct fh_token *tok)
{
  if (p->has_saved_tok) {
    *tok = p->saved_tok;
    p->has_saved_tok = 0;
    p->last_loc = tok->loc;
    //printf("::: re-token '%s'\n", fh_dump_token(p->t, tok));
    return 0;
  }

  if (fh_read_token(p->t, tok) < 0) {
    parse_error(p, fh_get_tokenizer_error_loc(p->t), "%s", fh_get_tokenizer_error(p->t));
    return -1;
  }
  p->last_loc = tok->loc;
  //printf(":::::: token '%s'\n", fh_dump_token(p->t, tok));
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

  if (op == NULL)
    return 1;
  
  const uint8_t *tok_op = fh_get_token_op(p->t, tok);
  if (tok_op != NULL && strcmp(op, (const char *) tok_op) == 0)
    return 1;
  return 0;
}

static void free_func(struct fh_p_expr_func func)
{
  if (func.params)
    free(func.params);
  free_block(func.body);
}

static void free_named_func(struct fh_p_named_func func)
{
  free_func(func.func);
}

static void free_expr_children(struct fh_p_expr *expr)
{
  switch (expr->type) {
  case EXPR_NONE: return;
  case EXPR_VAR: return;
  case EXPR_NUMBER: return;
  case EXPR_STRING: return;

  case EXPR_ASSIGN:
    free_expr(expr->data.assign.dest);
    free_expr(expr->data.assign.val);
    return;

  case EXPR_UN_OP:
    free_expr(expr->data.un_op.arg);
    return;

  case EXPR_BIN_OP:
    free_expr(expr->data.bin_op.left);
    free_expr(expr->data.bin_op.right);
    return;

  case EXPR_FUNC_CALL:
    free_expr(expr->data.func_call.func);
    if (expr->data.func_call.args) {
      for (int i = 0; i < expr->data.func_call.n_args; i++)
        free_expr_children(&expr->data.func_call.args[i]);
      free(expr->data.func_call.args);
    }
    return;

  case EXPR_FUNC:
    free_func(expr->data.func);
    return;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown expression type '%d'\n", expr->type);
}

static void free_expr(struct fh_p_expr *expr)
{
  if (expr) {
    free_expr_children(expr);
    free(expr);
  }
}

static void free_stmt_children(struct fh_p_stmt *stmt)
{
  switch (stmt->type) {
  case STMT_NONE: return;
  case STMT_EMPTY: return;
  case STMT_BREAK: return;
  case STMT_CONTINUE: return;

  case STMT_EXPR:
    free_expr(stmt->data.expr);
    return;

  case STMT_VAR_DECL:
    free_expr(stmt->data.decl.val);
    return;

  case STMT_BLOCK:
    free_block(stmt->data.block);
    return;

  case STMT_RETURN:
    free_expr(stmt->data.ret.val);
    return;

  case STMT_IF:
    free_expr(stmt->data.stmt_if.test);
    free_stmt(stmt->data.stmt_if.true_stmt);
    free_stmt(stmt->data.stmt_if.false_stmt);
    return;

  case STMT_WHILE:
    free_expr(stmt->data.stmt_while.test);
    free_stmt(stmt->data.stmt_while.stmt);
    return;
  }
  
  fprintf(stderr, "INTERNAL ERROR: unknown statement type '%d'\n", stmt->type);
}

static void free_stmt(struct fh_p_stmt *stmt)
{
  if (stmt) {
    free_stmt_children(stmt);
    free(stmt);
  }
}

static void free_stmts(struct fh_p_stmt **stmts, int n_stmts)
{
  if (stmts) {
    for (int i = 0; i < n_stmts; i++)
      free_stmt(stmts[i]);
  }
}

static void free_block(struct fh_p_stmt_block block)
{
  if (block.stmts) {
    free_stmts(block.stmts, block.n_stmts);
    free(block.stmts);
  }
}

static struct fh_p_expr *new_expr(struct fh_parser *p, struct fh_src_loc loc, enum fh_expr_type type)
{
  struct fh_p_expr *expr = malloc(sizeof(struct fh_p_expr));
  if (! expr)
    return parse_error_oom(p, loc);
  expr->type = type;
  return expr;
}

static struct fh_p_stmt *new_stmt(struct fh_parser *p, struct fh_src_loc loc, enum fh_stmt_type type)
{
  struct fh_p_stmt *stmt = malloc(sizeof(struct fh_p_stmt));
  if (! stmt)
    return parse_error_oom(p, loc);
  stmt->type = type;
  return stmt;
}

static int parse_arg_list(struct fh_parser *p, struct fh_p_expr **ret_args)
{
  struct fh_token tok;
  struct fh_stack args;

  fh_init_stack(&args, sizeof(struct fh_p_expr));

  if (get_token(p, &tok) < 0)
    goto err;

  if (! tok_is_punct(&tok, ')')) {
    unget_token(p, &tok);
    while (1) {
      struct fh_p_expr *e = parse_expr(p, false, ",)");
      if (! e)
        goto err;
      fh_push(&args, e);
      free(e);
      
      if (get_token(p, &tok) < 0)
        goto err;
      if (tok_is_punct(&tok, ')'))
        break;
      if (tok_is_punct(&tok, ','))
        continue;
      parse_error_expected(p, tok.loc, "',' or ')'");
      goto err;
    }
  }

  fh_stack_shrink_to_fit(&args);
  *ret_args = args.data;
  return args.num;
  
 err:
  for (int i = 0; i < args.num; i++) {
    struct fh_p_expr *e = fh_stack_item(&args, i);
    free_expr_children(e);
  }
  fh_free_stack(&args);
  return -1;
}

static void dump_opn_stack(struct fh_parser *p, struct fh_stack *opns)
{
  printf("**** opn stack has %d elements\n", opns->num);
  for (int i = 0; i < opns->num; i++) {
    struct fh_p_expr **pe = fh_stack_item(opns, i);
    printf("[%d] ", i);
    fh_dump_expr(&p->ast, NULL, *pe);
    printf("\n");
  }
}

static void dump_opr_stack(struct fh_parser *p, struct fh_stack *oprs)
{
  printf("**** opr stack has %d elements\n", oprs->num);
  for (int i = 0; i < oprs->num; i++) {
    struct fh_operator *op = fh_stack_item(oprs, i);
    printf("[%d] %s\n", i, op->name.str);
  }
}

static int resolve_expr_stack(struct fh_parser *p, struct fh_src_loc loc, struct fh_stack *opns, struct fh_stack *oprs, int32_t stop_prec)
{
  while (1) {
    //printf("********** STACKS ********************************\n");
    //dump_opn_stack(p, opns);
    //dump_opr_stack(p, oprs);
    //printf("**************************************************\n");
    
    if (fh_stack_is_empty(oprs))
      return 0;
    
    struct fh_operator *op = fh_stack_top(oprs);
    int32_t op_prec = (op->assoc == FH_ASSOC_RIGHT) ? op->prec-1 : op->prec;
    if (op_prec < stop_prec)
      return 0;
    uint32_t op_id = op->name.id;
    enum fh_op_assoc op_assoc = op->assoc;
    struct fh_p_expr *expr = new_expr(p, loc, EXPR_NONE);
    if (! expr) {
      parse_error_oom(p, loc);
      return -1;
    }
    
    fh_pop(oprs, NULL);
    switch (op_assoc) {
    case FH_ASSOC_RIGHT:
    case FH_ASSOC_LEFT:
      if (fh_stack_count(opns) < 2) {
        free_expr(expr);
        parse_error(p, loc, "syntax error");
        return -1;
      }
      const char *op_name = (const char *) fh_get_ast_op(&p->ast, op_id);
      if (strcmp(op_name, "=") == 0) {
        expr->type = EXPR_ASSIGN;
        fh_pop(opns, &expr->data.assign.val);
        fh_pop(opns, &expr->data.assign.dest);
      } else {
        expr->type = EXPR_BIN_OP;
        expr->data.bin_op.op = op_id;
        fh_pop(opns, &expr->data.bin_op.right);
        fh_pop(opns, &expr->data.bin_op.left);
      }
      break;

    case FH_ASSOC_PREFIX:
      if (fh_stack_count(opns) < 1) {
        free_expr(expr);
        parse_error(p, loc, "syntax error");
        return -1;
      }
      expr->type = EXPR_UN_OP;
      expr->data.bin_op.op = op_id;
      fh_pop(opns, &expr->data.un_op.arg);
      break;
    }

    if (expr->type == EXPR_NUMBER) {
      free_expr(expr);
      parse_error(p, loc, "bad operator assoc: %d", op_assoc);
      return -1;
    }
   
    if (fh_push(opns, &expr) < 0) {
      free_expr(expr);
      parse_error_oom(p, loc);
      return -1;
    }
  }
}

static struct fh_p_expr *parse_expr(struct fh_parser *p, bool consume_stop, char *stop_chars)
{
  struct fh_stack opns;
  struct fh_stack oprs;
  bool expect_opn = true;

  fh_init_stack(&opns, sizeof(struct fh_p_expr *));
  fh_init_stack(&oprs, sizeof(struct fh_operator));

  while (1) {
    struct fh_token tok;
    if (get_token(p, &tok) < 0)
      goto err;

    /* ( expr... ) */
    if (tok_is_punct(&tok, '(')) {
      struct fh_p_expr *expr;
      if (expect_opn) {
        expr = parse_expr(p, true, ")");
        if (expr == NULL)
          goto err;
        expect_opn = 0;
      } else {
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, FUNC_CALL_PREC) < 0)
          goto err;
        struct fh_p_expr *func;
        if (fh_pop(&opns, &func) < 0) {
          parse_error(p, tok.loc, "syntax error (no function on stack!)");
          goto err;
        }

        expr = new_expr(p, tok.loc, EXPR_FUNC_CALL);
        if (! expr) {
          free_expr(func);
          goto err;
        }
        expr->data.func_call.func = func;
        expr->data.func_call.n_args = parse_arg_list(p, &expr->data.func_call.args);
        if (expr->data.func_call.n_args < 0) {
          free(expr);
          free_expr(func);
          goto err;
        }
      }

      if (fh_push(&opns, &expr) < 0) {
        parse_error_oom(p, tok.loc);
        free_expr(expr);
        goto err;
      }
      continue;
    }

    /* stop char */
    if (stop_chars != NULL && tok.type == TOK_PUNCT) {
      bool is_stop = false;
      for (char *stop = stop_chars; *stop != '\0'; stop++) {
        if (*stop == tok.data.punct) {
          is_stop = true;
          break;
        }
      }
      if (is_stop) {
        if (! consume_stop)
          unget_token(p, &tok);

        //printf("BEFORE:\n"); dump_opn_stack(p, &opns);
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, INT32_MIN) < 0)
          goto err;
        //printf("AFTER:\n"); dump_opn_stack(p, &opns);

        if (fh_stack_count(&opns) > 1) {
          dump_opn_stack(p, &opns);
          dump_opr_stack(p, &oprs);
          parse_error(p, tok.loc, "syntax error (stack not empty!)");
          goto err;
        }
        struct fh_p_expr *ret;
        if (fh_pop(&opns, &ret) < 0) {
          parse_error_expected(p, tok.loc, "expression");
          goto err;
        }
        fh_free_stack(&opns);
        fh_free_stack(&oprs);
        return ret;
      }
    }

    /* operator */
    if (tok_is_op(p, &tok, NULL)) {
      if (expect_opn) {
        struct fh_operator *op = fh_get_prefix_op(&p->ast.op_table, tok.data.op_name);
        if (op == NULL) {
          parse_error_expected(p, tok.loc, "expression");
          goto err;
        }
        if (fh_push(&oprs, op) < 0) {
          parse_error_oom(p, tok.loc);
          goto err;
        }
      } else {
        struct fh_operator *op = fh_get_binary_op(&p->ast.op_table, tok.data.op_name);
        if (op == NULL) {
          parse_error_expected(p, tok.loc, "'(' or binary operator");
          goto err;
        }
        if (resolve_expr_stack(p, tok.loc, &opns, &oprs, op->prec) < 0)
          goto err;
        if (fh_push(&oprs, op) < 0) {
          parse_error_oom(p, tok.loc);
          goto err;
        }
        expect_opn = true;
      }
      continue;
    }
    
    /* number */
    if (tok_is_number(&tok)) {
      if (! expect_opn) {
        parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *num = new_expr(p, tok.loc, EXPR_NUMBER);
      if (! num)
        goto err;
      num->data.num = tok.data.num;
      if (fh_push(&opns, &num) < 0) {
        free_expr(num);
        parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* string */
    if (tok_is_string(&tok)) {
      if (! expect_opn) {
        parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *str = new_expr(p, tok.loc, EXPR_STRING);
      if (! str)
        goto err;
      str->data.str = tok.data.str;
      if (fh_push(&opns, &str) < 0) {
        free_expr(str);
        parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    /* variable */
    if (tok_is_symbol(&tok)) {
      if (! expect_opn) {
        parse_error_expected(p, tok.loc, "'(' or operator");
        goto err;
      }
      struct fh_p_expr *var = new_expr(p, tok.loc, EXPR_VAR);
      if (! var)
        goto err;
      var->data.var = tok.data.symbol_id;
      if (fh_push(&opns, &var) < 0) {
        free_expr(var);
        parse_error_oom(p, tok.loc);
        goto err;
      }
      expect_opn = false;
      continue;
    }

    parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->t, &tok));
    goto err;
  }

 err:
  for (int i = 0; i < opns.num; i++) {
    struct fh_p_expr **pe = fh_stack_item(&opns, i);
    free_expr(*pe);
  }
  fh_free_stack(&opns);
  fh_free_stack(&oprs);
  return NULL;
}

static struct fh_p_stmt *parse_stmt_if(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return parse_error_expected(p, tok.loc, "'('");

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
  free_stmt(stmt);
  return NULL;
}

static struct fh_p_stmt *parse_stmt_while(struct fh_parser *p)
{
  struct fh_token tok;
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return parse_error_expected(p, tok.loc, "'('");

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
  free_stmt(stmt);
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
      parse_error_expected(p, tok.loc, "';'");
    stmt->type = STMT_BREAK;
    return stmt;
  }

  // continue;
  if (tok_is_keyword(&tok, KW_CONTINUE)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (! tok_is_punct(&tok, ';'))
      parse_error_expected(p, tok.loc, "';'");
    stmt->type = STMT_CONTINUE;
    return stmt;
  }
  
  // var name [= expr] ;
  if (tok_is_keyword(&tok, KW_VAR)) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (! tok_is_symbol(&tok))
      return parse_error_expected(p, tok.loc, "variable name");
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
      parse_error_expected(p, tok.loc, "'=' or ';'");
      goto err;
    }
    stmt->type = STMT_VAR_DECL;
    return stmt;
  }

  // { ... }
  if (tok_is_punct(&tok, '{')) {
    unget_token(p, &tok);
    if (parse_block(p, &stmt->data.block) == NULL)
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
    return parse_error_expected(p, tok.loc, "'{'");

  struct fh_stack stmts;
  fh_init_stack(&stmts, sizeof(struct fh_stmt *));
  while (1) {
    if (get_token(p, &tok) < 0)
      goto err;
    if (tok_is_punct(&tok, '}'))
      break;
    unget_token(p, &tok);

    struct fh_p_stmt *stmt = parse_stmt(p);
    if (stmt == NULL)
      goto err;
    if (fh_push(&stmts, &stmt) < 0) {
      parse_error_oom(p, tok.loc);
      goto err;
    }
  }

  if (fh_stack_shrink_to_fit(&stmts) < 0)
    goto err;
  block->n_stmts = stmts.num;
  block->stmts = stmts.data;
  return block;

 err:
  free_stmts(stmts.data, stmts.num);
  fh_free_stack(&stmts);
  return NULL;
}

static struct fh_p_expr_func *parse_func(struct fh_parser *p, struct fh_p_expr_func *func)
{
  struct fh_token tok;

  // param list
  if (get_token(p, &tok) < 0)
    return NULL;
  if (! tok_is_punct(&tok, '('))
    return parse_error_expected(p, tok.loc, "'('");
  if (get_token(p, &tok) < 0)
    return NULL;
  fh_symbol_id params[64];
  int n_params = 0;
  if (! tok_is_punct(&tok, ')')) {
    while (1) {
      if (! tok_is_symbol(&tok))
        return parse_error_expected(p, tok.loc, "name");
      if (n_params >= sizeof(params)/sizeof(params[0]))
        return parse_error(p, tok.loc, "too many parameters");
      params[n_params++] = tok.data.symbol_id;
      
      if (get_token(p, &tok) < 0)
        return NULL;
      if (tok_is_punct(&tok, ')'))
        break;
      if (! tok_is_punct(&tok, ','))
        return parse_error_expected(p, tok.loc, "')' or ','");
      if (get_token(p, &tok) < 0)
        return NULL;
    }
  }
  
  // function body
  if (parse_block(p, &func->body) == NULL)
    return NULL;

  // copy params
  func->n_params = n_params;
  func->params = malloc(n_params * sizeof(params[0]));
  if (func->params == NULL) {
    free_block(func->body);
    return parse_error_oom(p, tok.loc);
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
    return parse_error_expected(p, tok.loc, "function name");
  func->name = tok.data.symbol_id;

  // rest of function
  if (parse_func(p, &func->func) == NULL)
    return NULL;

  return func;
}

int fh_parse(struct fh_parser *p)
{
  struct fh_token tok;
  struct fh_stack funcs;

  fh_init_stack(&funcs, sizeof(struct fh_p_named_func));
  while (1) {
    if (get_token(p, &tok) < 0)
      goto error;

    if (tok_is_eof(&tok))
      break;
    
    if (tok_is_keyword(&tok, KW_FUNCTION)) {
      struct fh_p_named_func func;
      if (parse_named_func(p, &func) == NULL)
        goto error;
      if (fh_push(&funcs, &func) < 0) {
        parse_error_oom(p, tok.loc);
        goto error;
      }
      continue;
    }

    parse_error(p, tok.loc, "unexpected '%s'", fh_dump_token(p->t, &tok));
    goto error;
  }
  for (int i = 0; i < funcs.num; i++) {
    struct fh_p_named_func *f = fh_stack_item(&funcs, i);
    fh_push(&p->ast.funcs, f);
  }
  fh_free_stack(&funcs);
  return 0;

 error:
  for (int i = 0; i < funcs.num; i++) {
    struct fh_p_named_func *f = fh_stack_item(&funcs, i);
    free_named_func(*f);
  }
  fh_free_stack(&funcs);
  return -1;
}

void fh_parser_dump(struct fh_parser *p)
{
  for (int i = 0; i < p->ast.funcs.num; i++) {
    struct fh_p_named_func *f = fh_stack_item(&p->ast.funcs, i);
    fh_dump_named_func(&p->ast, NULL, f);
  }
}
