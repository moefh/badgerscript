/* tokenizer.h */

#ifndef TOKENIZER_H_FILE
#define TOKENIZER_H_FILE

#include "fh_internal.h"

#define TOKENIZER_BUF_SIZE 256

enum fh_token_type {
  TOK_EOF,
  TOK_KEYWORD,
  TOK_SYMBOL,
  TOK_STRING,
  TOK_NUMBER,
  TOK_OP,
  TOK_PUNCT,
};

enum fh_keyword_type {
  KW_INCLUDE,
  KW_FUNCTION,
  KW_RETURN,
  KW_VAR,
  KW_IF,
  KW_ELSE,
  KW_WHILE,
  KW_BREAK,
  KW_CONTINUE,
};

struct fh_token {
  enum fh_token_type type;
  struct fh_src_loc loc;
  union {
    double num;
    fh_string_id str;
    enum fh_keyword_type keyword;
    fh_symbol_id symbol_id;
    char op_name[4];
    uint32_t punct;
  } data;
};

struct fh_tokenizer {
  struct fh_tokenizer *next;
  struct fh_program *prog;
  struct fh_input *in;
  struct fh_ast *ast;
  struct fh_buffer *tmp;
  uint16_t file_id;

  struct fh_src_loc cur_loc;
  uint32_t buf_pos;
  uint32_t buf_len;
  char buf[TOKENIZER_BUF_SIZE];

  struct fh_src_loc last_err_loc;
  
  int saved_byte;
  struct fh_src_loc saved_loc;
};

struct fh_tokenizer *fh_new_tokenizer(struct fh_program *prog, struct fh_input *in, struct fh_ast *ast, struct fh_buffer *tmp_buf, uint16_t file_id);
void fh_free_tokenizer(struct fh_tokenizer *t);
int fh_read_token(struct fh_tokenizer *t, struct fh_token *tok);
struct fh_src_loc fh_get_tokenizer_error_loc(struct fh_tokenizer *t);
const char *fh_get_token_keyword(struct fh_token *tok);
const char *fh_get_token_symbol(struct fh_ast *ast, struct fh_token *tok);
const char *fh_get_token_string(struct fh_ast *ast, struct fh_token *tok);
const char *fh_get_token_op(struct fh_token *tok);

const char *fh_dump_token(struct fh_ast *ast, struct fh_token *tok);

#define tok_is_eof(tok)          ((tok)->type == TOK_EOF)
#define tok_is_number(tok)       ((tok)->type == TOK_NUMBER)
#define tok_is_string(tok)       ((tok)->type == TOK_STRING)
#define tok_is_punct(tok, p)     ((tok)->type == TOK_PUNCT && (tok)->data.punct == (p))
#define tok_is_keyword(tok, kw)  ((tok)->type == TOK_KEYWORD && (tok)->data.keyword == (kw))
#define tok_is_symbol(tok)       ((tok)->type == TOK_SYMBOL)

#endif /* TOKENIZER_H_FILE */
