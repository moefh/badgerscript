/* fh_internal.h */

#ifndef FH_INTERNAL_H_FILE
#define FH_INTERNAL_H_FILE

#include "fh.h"
#include "stack.h"

#define ARRAY_SIZE(arr)  ((int)(sizeof(arr)/sizeof(arr[0])))
#define UNUSED(x)        ((void)(x))

enum fh_op_assoc {
  FH_ASSOC_PREFIX = (1<<0),
  FH_ASSOC_LEFT   = (1<<1),
  FH_ASSOC_RIGHT  = (1<<2),
};

struct fh_src_loc {
  uint16_t line;
  uint16_t col;
  uint16_t file_id;
};

#define fh_make_src_loc(f, l, c) ((struct fh_src_loc) { .file_id = (f), .line = (l), .col = (c) })

struct fh_buffer {
  char *p;
  int size;
  int cap;
};

struct fh_operator {
  uint32_t op;
  char name[4];
  enum fh_op_assoc assoc;
  int32_t prec;
};

struct fh_symtab {
  int num;
  int cap;
  int *entries;
  struct fh_buffer symbols;
};

typedef int32_t fh_symbol_id;
typedef int fh_string_id;

struct fh_value;
struct fh_vm;
struct fh_func_def;
struct fh_closure;
struct fh_symtab;
struct fh_tokenizer;
struct fh_parser;
struct fh_compiler;
struct fh_program;
struct fh_map;

extern const struct fh_named_c_func fh_std_c_funcs[];
extern const int fh_std_c_funcs_len;

uint32_t fh_hash(const void *data, size_t len);
int fh_utf8_len(char *str, size_t str_size);
void fh_dump_value(const struct fh_value *val);
void fh_dump_string(const char *str);
void fh_dump_map(struct fh_map *map);
const void *fh_decode_src_loc(const void *encoded, int encoded_len, struct fh_src_loc *src_loc, int n_instr);
int fh_encode_src_loc_change(struct fh_buffer *buf, struct fh_src_loc *old_loc, struct fh_src_loc *new_loc);
struct fh_src_loc fh_get_addr_src_loc(struct fh_func_def *func_def, int addr);

void fh_init_buffer(struct fh_buffer *buf);
void fh_destroy_buffer(struct fh_buffer *buf);
int fh_buf_grow(struct fh_buffer *buf, size_t add_size);
int fh_buf_shrink_to_fit(struct fh_buffer *buf);
int fh_buf_add_string(struct fh_buffer *buf, const void *str, size_t str_size);
int fh_buf_add_byte(struct fh_buffer *buf, uint8_t c);
int fh_buf_add_u16(struct fh_buffer *buf, uint16_t c);

void fh_init_symtab(struct fh_symtab *s);
void fh_destroy_symtab(struct fh_symtab *s);
fh_symbol_id fh_add_symbol(struct fh_symtab *s, const void *symbol);
fh_symbol_id fh_get_symbol_id(struct fh_symtab *s, const void *symbol);
const char *fh_get_symbol_name(struct fh_symtab *s, fh_symbol_id id);

struct fh_operator *fh_get_binary_op(const char *name);
struct fh_operator *fh_get_prefix_op(const char *name);
struct fh_operator *fh_get_op(const char *name);
struct fh_operator *fh_get_op_by_id(uint32_t op);
const char *fh_get_op_name(uint32_t op);

void fh_free_program_objects(struct fh_program *prog);
int fh_get_pin_state(struct fh_program *prog);
void fh_restore_pin_state(struct fh_program *prog, int state);

fh_c_func fh_get_c_func_by_name(struct fh_program *prog, const char *name);
const char *fh_get_c_func_name(struct fh_program *prog, fh_c_func func);

int fh_add_global_func(struct fh_program *prog, struct fh_closure *closure);
int fh_get_num_global_funcs(struct fh_program *prog);
struct fh_closure *fh_get_global_func_by_index(struct fh_program *prog, int index);
struct fh_closure *fh_get_global_func_by_name(struct fh_program *prog, const char *name);

#endif /* FH_INTERNAL_H_FILE */
