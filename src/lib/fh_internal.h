/* fh_internal.h */

#ifndef FH_INTERNAL_H_FILE
#define FH_INTERNAL_H_FILE

#include "fh.h"
#include "stack.h"

#include <stdbool.h>

#define ARRAY_SIZE(arr)  ((int)(sizeof(arr)/sizeof(arr[0])))
#define UNUSED(x) ((void)(x))

enum fh_op_assoc {
  FH_ASSOC_PREFIX,
  FH_ASSOC_LEFT,
  FH_ASSOC_RIGHT,
};

struct fh_src_loc {
  uint16_t line;
  uint16_t col;
};

struct fh_buffer {
  char *p;
  int size;
  int cap;
};

struct fh_operator {
  enum fh_op_assoc assoc;
  int32_t prec;
  uint32_t op;
  char name[4];
};

DECLARE_STACK(op_stack, struct fh_operator);

struct fh_op_table {
  struct op_stack prefix;
  struct op_stack binary;
};

typedef int32_t fh_symbol_id;
typedef int fh_string_id;

struct fh_value;
struct fh_vm;
struct fh_func;
struct fh_output;
struct fh_symtab;
struct fh_tokenizer;
struct fh_parser;
struct fh_compiler;
struct fh_program;

int fh_utf8_len(char *str, size_t str_size);
struct fh_src_loc fh_make_src_loc(uint16_t line, uint16_t col);
void fh_output(struct fh_output *out, char *fmt, ...) __attribute__((format (printf, 2, 3)));
void fh_dump_value(const struct fh_value *val);
void fh_dump_string(const char *str);

void fh_init_buffer(struct fh_buffer *buf);
void fh_free_buffer(struct fh_buffer *buf);
int fh_buf_grow(struct fh_buffer *buf, size_t add_size);
int fh_buf_add_string(struct fh_buffer *buf, const char *str, size_t str_size);
int fh_buf_add_byte(struct fh_buffer *buf, uint8_t c);

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

void fh_free_program_objects(struct fh_program *prog);

fh_c_func fh_get_c_func_by_name(struct fh_program *prog, const char *name);
const char *fh_get_c_func_name(struct fh_program *prog, fh_c_func func);

int fh_add_func(struct fh_program *prog, struct fh_func *func, bool is_global);
int fh_get_num_funcs(struct fh_program *prog);
struct fh_func *fh_get_func(struct fh_program *prog, int num);
struct fh_func *fh_get_global_func(struct fh_program *prog, const char *name);

#endif /* FH_INTERNAL_H_FILE */
