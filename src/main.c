/* main.c */

#include <stdio.h>

#include "lib/fh_i.h"
#include "lib/ast.h"
#include "lib/bytecode.h"

static int my_printf(struct fh_vm *vm, struct fh_value *ret, struct fh_value *args, int n_args)
{
  UNUSED(vm);
  UNUSED(args);
  printf("Called C function with %d args:\n", n_args);
  for (int i = 0; i < n_args; i++) {
    printf("- ");
    fh_dump_value(&args[i]);
    printf("\n");
  }
  fh_make_number(ret, 42);
  return 0;
}

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
  fh_compiler_add_c_func(c, "printf", my_printf);
  printf("-> compiling...\n");
  if (fh_compile(c) < 0) {
    printf("%s:%s\n", filename, fh_get_compiler_error(c));
    goto err;
  }
  fh_dump_bc(bc, NULL);

  // run
  printf("-> calling function 'main'...\n");
  vm = fh_new_vm(bc);
  if (! vm)
    goto err;
  struct fh_value args[7];
  fh_make_number(&args[0], -2);
  fh_make_number(&args[1], -2);
  fh_make_number(&args[2], 2);
  fh_make_number(&args[3], 2);
  fh_make_number(&args[4], 80);
  fh_make_number(&args[5], 40);
  fh_make_number(&args[6], 10);
  struct fh_value ret;
  if (fh_call_vm_func(vm, "main", args, 7, &ret) < 0) {
    printf("ERROR: %s\n", fh_get_vm_error(vm));
    goto err;
  }
  printf("-> returned value ");
  fh_dump_value(&ret);
  printf("\n");

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
