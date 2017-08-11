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
DECLARE_STACK(p_func_stack, struct fh_func *);

struct fh_program {
  char last_error_msg[256];
  struct fh_value null_value;
  struct fh_parser parser;
  struct fh_compiler compiler;
  struct named_c_func_stack c_funcs;
  struct fh_vm vm;            // VM stack contains GC roots (stack values)
  struct p_func_stack funcs;  // GC roots (global functions)
  struct value_stack c_vals;  // GC roots (values held by running C functions)
  struct fh_object *objects;  // all created objects
};

#endif /* PROGRAM_H_FILE */
