/* parser.h */

#ifndef PARSER_H_FILE
#define PARSER_H_FILE

#include "fh_internal.h"
#include "tokenizer.h"

struct fh_ast;

struct fh_parser {
  struct fh_program *prog;
  struct fh_mem_pool *pool;
  struct fh_tokenizer *tokenizer;
  struct fh_ast *ast;
  struct fh_buffer tmp_buf;
  struct fh_src_loc last_loc;
  int has_saved_tok;
  struct fh_token saved_tok;
};

void fh_init_parser(struct fh_parser *p, struct fh_program *prog, struct fh_mem_pool *pool);
void fh_destroy_parser(struct fh_parser *p);
int fh_parse(struct fh_parser *p, struct fh_ast *ast, struct fh_input *in);
void *fh_parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...) FH_PRINTF_FORMAT(3, 4);
void *fh_parse_error_oom(struct fh_parser *p, struct fh_src_loc loc);
void *fh_parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected);

#endif /* PARSER_H_FILE */
