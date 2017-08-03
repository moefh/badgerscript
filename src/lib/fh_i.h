/* fh_i.h */

#ifndef FH_I_H_FILE
#define FH_I_H_FILE

#include <stdint.h>
#include <stdbool.h>

#include "fh.h"
#include "stack.h"

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
  KW_FUNCTION,
  KW_RETURN,
  KW_VAR,
  KW_IF,
  KW_ELSE,
  KW_WHILE,
  KW_BREAK,
  KW_CONTINUE,
};

struct fh_buffer {
  uint8_t *p;
  size_t size;
  size_t cap;
};

struct fh_operator {
  enum fh_op_assoc assoc;
  int32_t prec;
  uint32_t op;
  char name[4];
};

struct fh_op_table {
  struct fh_stack prefix;
  struct fh_stack binary;
};

typedef int32_t fh_symbol_id;
typedef ssize_t fh_string_id;

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

struct fh_bc;
struct fh_output;
struct fh_symtab;
struct fh_tokenizer;
struct fh_parser;
struct fh_compiler;

struct fh_ast {
  struct fh_buffer string_pool;
  struct fh_symtab *symtab;
  struct fh_op_table op_table;
  struct fh_stack funcs;
};

ssize_t fh_utf8_len(uint8_t *str, size_t str_size);
struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col);
const char *fh_dump_token(struct fh_tokenizer *t, struct fh_token *tok);
void fh_dump_mem(const char *label, const void *data, size_t len);
void fh_output(struct fh_output *out, char *fmt, ...) __attribute__((format (printf, 2, 3)));

void fh_init_buffer(struct fh_buffer *buf);
void fh_free_buffer(struct fh_buffer *buf);
int fh_buf_grow(struct fh_buffer *buf, size_t add_size);
ssize_t fh_buf_add_string(struct fh_buffer *buf, uint8_t *str, size_t str_size);
ssize_t fh_buf_add_byte(struct fh_buffer *buf, uint8_t c);

struct fh_symtab *fh_symtab_new(void);
void fh_symtab_free(struct fh_symtab *s);
fh_symbol_id fh_symtab_add(struct fh_symtab *s, uint8_t *symbol);
fh_symbol_id fh_symtab_get_id(struct fh_symtab *s, uint8_t *symbol);
const uint8_t *fh_symtab_get_symbol(struct fh_symtab *s, fh_symbol_id id);

void fh_init_op_table(struct fh_op_table *ops);
void fh_free_op_table(struct fh_op_table *ops);
int fh_add_op(struct fh_op_table *ops, uint32_t op, char *name, int32_t prec, enum fh_op_assoc assoc);
struct fh_operator *fh_get_binary_op(struct fh_op_table *ops, char *name);
struct fh_operator *fh_get_prefix_op(struct fh_op_table *ops, char *name);
struct fh_operator *fh_get_op(struct fh_op_table *ops, char *name);

struct fh_tokenizer *fh_new_tokenizer(struct fh_input *in, struct fh_ast *ast);
void fh_free_tokenizer(struct fh_tokenizer *t);
int fh_read_token(struct fh_tokenizer *t, struct fh_token *tok);
const uint8_t *fh_get_tokenizer_error(struct fh_tokenizer *t);
struct fh_src_loc fh_get_tokenizer_error_loc(struct fh_tokenizer *t);
const uint8_t *fh_get_token_keyword(struct fh_tokenizer *t, struct fh_token *tok);
const uint8_t *fh_get_token_symbol(struct fh_tokenizer *t, struct fh_token *tok);
const uint8_t *fh_get_token_string(struct fh_tokenizer *t, struct fh_token *tok);
const uint8_t *fh_get_token_op(struct fh_tokenizer *t, struct fh_token *tok);

struct fh_parser *fh_new_parser(struct fh_input *in, struct fh_ast *ast);
void fh_free_parser(struct fh_parser *p);
int fh_parse(struct fh_parser *p);
const uint8_t *fh_get_parser_error(struct fh_parser *p);
void fh_parser_dump(struct fh_parser *p);
void *fh_parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
void *fh_parse_error_oom(struct fh_parser *p, struct fh_src_loc loc);
void *fh_parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected);

struct fh_compiler *fh_new_compiler(struct fh_ast *ast, struct fh_bc *bc);
void fh_free_compiler(struct fh_compiler *c);
int fh_compile(struct fh_compiler *c);
const uint8_t *fh_get_compiler_error(struct fh_compiler *p);
int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
uint32_t *fh_get_compiler_instructions(struct fh_compiler *c, int32_t *len);

void fh_dump_bc(struct fh_bc *bc, struct fh_output *out);

#endif /* FH_I_H_FILE */
