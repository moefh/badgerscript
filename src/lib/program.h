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

struct fh_program {
  char last_error_msg[256];
  struct fh_value null_value;
  struct fh_parser parser;
  struct fh_compiler compiler;
  struct fh_bc bc;            // BC funcs contains GC roots (function constants)
  struct fh_vm vm;            // VM stack contains GC roots (stack values)
  struct fh_stack c_vals;     // GC roots (values held by running C functions)
  struct fh_object *objects;  // all created objects
};

#endif /* PROGRAM_H_FILE */
