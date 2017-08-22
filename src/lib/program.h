/* program.h */

#ifndef PROGRAM_H_FILE
#define PROGRAM_H_FILE

#include "fh.h"
#include "ast.h"
#include "bytecode.h"
#include "vm.h"
#include "parser.h"
#include "compiler.h"
#include "value.h"

struct named_c_func {
  const char *name;
  fh_c_func func;
};

DECLARE_STACK(named_c_func_stack, struct named_c_func);
DECLARE_STACK(p_closure_stack, struct fh_closure *);
DECLARE_STACK(p_object_stack, struct fh_object *);

struct fh_program {
  char last_error_msg[256];
  int gc_frequency;
  int n_created_objs_since_last_gc;
  struct fh_value null_value;
  struct fh_symtab src_file_names;
  struct named_c_func_stack c_funcs;
  struct fh_vm vm;                       // GC roots (VM stack)
  struct p_closure_stack global_funcs;   // GC roots (global functions)
  struct p_object_stack pinned_objs;     // GC roots (temporarily pinned objects)
  struct value_stack c_vals;             // GC roots (values held by running C functions)
  struct fh_object *objects;             // all created objects
};

#endif /* PROGRAM_H_FILE */
