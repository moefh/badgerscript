/* main.c */

#include <stdio.h>

#include "lib/fh_i.h"
#include "lib/ast.h"
#include "lib/bytecode.h"

static int run_file(const char *filename)
{
  struct fh_input *in;
  struct fh_ast *ast = NULL;
  struct fh_parser *p = NULL;
  struct fh_bc *bc = NULL;
  struct fh_compiler *c = NULL;
  struct fh_vm *vm = NULL;

  printf("-> opening input file...\n");
  in = fh_open_input_file(filename);
  if (! in) {
    printf("ERROR: can't open '%s'\n", filename);
    goto err;
  }
  
  // parse
  ast = fh_new_ast();
  if (! ast) {
    printf("ERROR: out of memory for AST\n");
    goto err;
  }
  p = fh_new_parser(in, ast);
  if (! p) {
    printf("ERROR: out of memory for parser\n");
    goto err;
  }

  printf("-> parsing...\n");
  if (fh_parse(p) < 0) {
    printf("%s:%s\n", filename, fh_get_parser_error(p));
    goto err;
  }
  fh_parser_dump(p);

  // compile
  bc = fh_new_bc();
  if (! bc) {
    printf("ERROR: out of memory for bytecode\n");
    goto err;
  }
  c = fh_new_compiler(ast, bc);
  if (! c) {
    printf("ERROR: out of memory for compiler\n");
    goto err;
  }
  printf("-> compiling...\n");
  if (fh_compile(c) < 0) {
    printf("%s:%s\n", filename, fh_get_compiler_error(c));
    goto err;
  }
  fh_dump_bc(bc, NULL);
  if (fh_get_bc_num_funcs(bc) == 0) {
    printf("ERROR: no functions!\n");
    goto err;
  }

  // run
  printf("-> running...\n");
  vm = fh_new_vm(bc);
  if (! vm)
    goto err;
  struct fh_bc_func *f = fh_get_bc_func(bc, fh_get_bc_num_funcs(bc)-1);
  if (fh_run_vm_func(vm, f) < 0) {
    printf("ERROR running bytecode\n");
    goto err;
  }

  fh_free_vm(vm);
  fh_free_compiler(c);
  fh_free_bc(bc);
  fh_free_parser(p);
  fh_free_ast(ast);
  fh_close_input(in);
  return 0;
  
 err:
  if (vm)
    fh_free_vm(vm);
  if (c)
    fh_free_compiler(c);
  if (bc)
    fh_free_bc(bc);
  if (p)
    fh_free_parser(p);
  if (ast)
    fh_free_ast(ast);
  if (in)
    fh_close_input(in);
  return 1;
}

int main(int argc, char **argv)
{
  if (argc <= 1) {
    printf("USAGE: %s filename\n", argv[0]);
    return 1;
  }

  return run_file(argv[1]);
}
