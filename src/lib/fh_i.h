/* fh_i.h */

#ifndef FH_I_H_FILE
#define FH_I_H_FILE

#include <stdint.h>
#include <stdbool.h>

#include "fh.h"
#include "stack.h"

#define ARRAY_SIZE(arr)  ((int)(sizeof(arr)/sizeof(arr[0])))
#define UNUSED(x) ((void)(x))

enum fh_op_assoc {
  FH_ASSOC_PREFIX,
  FH_ASSOC_LEFT,
  FH_ASSOC_RIGHT,
};

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

struct fh_src_loc {
  uint16_t line;
  uint16_t col;
};

struct fh_buffer {
  char *p;
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

struct fh_value;
struct fh_vm;
struct fh_bc_func;
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

ssize_t fh_utf8_len(char *str, size_t str_size);
struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col);
const char *fh_dump_token(struct fh_tokenizer *t, struct fh_token *tok);
void fh_output(struct fh_output *out, char *fmt, ...) __attribute__((format (printf, 2, 3)));
void fh_dump_mem(const char *label, const void *data, size_t len);
void fh_dump_value(const struct fh_value *val);
void fh_make_number(struct fh_value *val, double num);
void fh_make_c_func(struct fh_value *val, fh_c_func func);

void fh_init_buffer(struct fh_buffer *buf);
void fh_free_buffer(struct fh_buffer *buf);
int fh_buf_grow(struct fh_buffer *buf, size_t add_size);
ssize_t fh_buf_add_string(struct fh_buffer *buf, const char *str, size_t str_size);
ssize_t fh_buf_add_byte(struct fh_buffer *buf, uint8_t c);

struct fh_symtab *fh_new_symtab(void);
void fh_free_symtab(struct fh_symtab *s);
fh_symbol_id fh_add_symbol(struct fh_symtab *s, const char *symbol);
fh_symbol_id fh_get_symbol_id(struct fh_symtab *s, const char *symbol);
const char *fh_get_symbol_name(struct fh_symtab *s, fh_symbol_id id);

void fh_init_op_table(struct fh_op_table *ops);
void fh_free_op_table(struct fh_op_table *ops);
int fh_add_op(struct fh_op_table *ops, uint32_t op, char *name, int32_t prec, enum fh_op_assoc assoc);
struct fh_operator *fh_get_binary_op(struct fh_op_table *ops, char *name);
struct fh_operator *fh_get_prefix_op(struct fh_op_table *ops, char *name);
struct fh_operator *fh_get_op(struct fh_op_table *ops, char *name);
struct fh_operator *fh_get_op_by_id(struct fh_op_table *ops, uint32_t op);

struct fh_tokenizer *fh_new_tokenizer(struct fh_input *in, struct fh_ast *ast);
void fh_free_tokenizer(struct fh_tokenizer *t);
int fh_read_token(struct fh_tokenizer *t, struct fh_token *tok);
const char *fh_get_tokenizer_error(struct fh_tokenizer *t);
struct fh_src_loc fh_get_tokenizer_error_loc(struct fh_tokenizer *t);
const char *fh_get_token_keyword(struct fh_tokenizer *t, struct fh_token *tok);
const char *fh_get_token_symbol(struct fh_tokenizer *t, struct fh_token *tok);
const char *fh_get_token_string(struct fh_tokenizer *t, struct fh_token *tok);
const char *fh_get_token_op(struct fh_tokenizer *t, struct fh_token *tok);

struct fh_parser *fh_new_parser(void);
void fh_free_parser(struct fh_parser *p);
int fh_parse(struct fh_parser *p, struct fh_ast *ast, struct fh_input *in);
const char *fh_get_parser_error(struct fh_parser *p);
void *fh_parse_error(struct fh_parser *p, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
void *fh_parse_error_oom(struct fh_parser *p, struct fh_src_loc loc);
void *fh_parse_error_expected(struct fh_parser *p, struct fh_src_loc loc, char *expected);

struct fh_compiler *fh_new_compiler(void);
void fh_free_compiler(struct fh_compiler *c);
int fh_compile(struct fh_compiler *c, struct fh_bc *bc, struct fh_ast *ast);
const char *fh_get_compiler_error(struct fh_compiler *p);
int fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...) __attribute__((format(printf, 3, 4)));
int fh_compiler_add_c_func(struct fh_compiler *c, const char *name, fh_c_func func);
uint32_t *fh_get_compiler_instructions(struct fh_compiler *c, int32_t *len);

int fh_run_vm(struct fh_vm *vm);

#endif /* FH_I_H_FILE */
