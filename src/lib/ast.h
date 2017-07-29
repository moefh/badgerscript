/* parser.h */

#ifndef PARSER_H_FILE
#define PARSER_H_FILE

#include "fh_i.h"

/* =========================================== */
/* == statements ============================= */

enum fh_stmt_type {
  STMT_NONE,
  STMT_EMPTY,
  STMT_VAR_DECL,
  STMT_EXPR,
  STMT_BLOCK,
  STMT_RETURN,
  STMT_IF,
  STMT_WHILE,
  STMT_BREAK,
  STMT_CONTINUE,
};

struct fh_p_stmt_decl {
  fh_symbol_id var;
  struct fh_p_expr *val;
};

struct fh_p_stmt_return {
  struct fh_p_expr *val;
};

struct fh_p_stmt_if {
  struct fh_p_expr *test;
  struct fh_p_stmt *true_stmt;
  struct fh_p_stmt *false_stmt;
};

struct fh_p_stmt_while {
  struct fh_p_expr *test;
  struct fh_p_stmt *stmt;
};

struct fh_p_stmt_block {
  int n_stmts;
  struct fh_p_stmt **stmts;
};

struct fh_p_stmt {
  enum fh_stmt_type type;
  struct fh_src_loc loc;
  union {
    struct fh_p_stmt_decl decl;
    struct fh_p_stmt_block block;
    struct fh_p_stmt_return ret;
    struct fh_p_stmt_if stmt_if;
    struct fh_p_stmt_while stmt_while;
    struct fh_p_expr *expr;
  } data;
};

/* =========================================== */
/* == expressions ============================ */

enum fh_expr_type {
  EXPR_NONE,
  EXPR_VAR,
  EXPR_NUMBER,
  EXPR_STRING,
  EXPR_ASSIGN,
  EXPR_BIN_OP,
  EXPR_UN_OP,
  EXPR_FUNC,
  EXPR_FUNC_CALL,
};

struct fh_p_expr_assign {
  struct fh_p_expr *dest;
  struct fh_p_expr *val;
};

struct fh_p_expr_bin_op {
  uint32_t op;
  struct fh_p_expr *left;
  struct fh_p_expr *right;
};

struct fh_p_expr_un_op {
  uint32_t op;
  struct fh_p_expr *arg;
};

struct fh_p_expr_func_call {
  struct fh_p_expr *func;
  int n_args;
  struct fh_p_expr *args;
};

struct fh_p_expr_func {
  int n_params;
  fh_symbol_id *params;
  struct fh_p_stmt_block body;
};

struct fh_p_expr {
  enum fh_expr_type type;
  struct fh_src_loc loc;
  union {
    fh_symbol_id var;
    double num;
    fh_string_id str;
    struct fh_p_expr_assign assign;
    struct fh_p_expr_bin_op bin_op;
    struct fh_p_expr_un_op un_op;
    struct fh_p_expr_func func;
    struct fh_p_expr_func_call func_call;
  } data;
};

/* =========================================== */
/* == named function ========================= */

struct fh_p_named_func {
  fh_symbol_id name;
  struct fh_src_loc loc;
  struct fh_p_expr_func func;
};

/* =========================================== */

struct fh_output;
void fh_dump_named_func(struct fh_ast *ast, struct fh_output *out, struct fh_p_named_func *func);
void fh_dump_expr(struct fh_ast *ast, struct fh_output *out, struct fh_p_expr *expr);

#endif /* PARSER_H_FILE */
