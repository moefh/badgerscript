/* fh.c */

#include <stdlib.h>
#include <stdio.h>

#include "fh.h"
#include "ast.h"
#include "bytecode.h"
#include "vm.h"
#include "parser.h"
#include "compiler.h"

int fh_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

struct fh_program {
  struct fh_parser parser;
  struct fh_compiler compiler;
  struct fh_bc bc;
  struct fh_vm vm;
  char last_error_msg[256];
};

struct fh_program *fh_new_program(void)
{
  struct fh_program *prog = malloc(sizeof(struct fh_program));
  if (! prog)
    return NULL;
  prog->last_error_msg[0] = '\0';

  if (fh_init_parser(&prog->parser, prog) < 0)
    goto err1;
  if (fh_init_compiler(&prog->compiler, prog) < 0)
    goto err2;
  if (fh_init_bc(&prog->bc) < 0)
    goto err3;
  if (fh_init_vm(&prog->vm, prog, &prog->bc) < 0)
    goto err4;

  if (fh_add_c_func(prog, "printf", fh_printf) < 0)
    goto err;
  
  return prog;

 err:
 err4:
  fh_destroy_bc(&prog->bc);
 err3:
  fh_destroy_compiler(&prog->compiler);
 err2:
  fh_destroy_parser(&prog->parser);  
 err1:
  free(prog);
  return NULL;
}

void fh_free_program(struct fh_program *prog)
{
  fh_destroy_vm(&prog->vm);
  fh_destroy_bc(&prog->bc);
  fh_destroy_compiler(&prog->compiler);
  fh_destroy_parser(&prog->parser);
  free(prog);
}

const char *fh_get_error(struct fh_program *prog)
{
  return prog->last_error_msg;
}

int fh_set_error(struct fh_program *prog, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(prog->last_error_msg, sizeof(prog->last_error_msg), fmt, ap);
  va_end(ap);

  return -1;
}

int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap)
{
  vsnprintf(prog->last_error_msg, sizeof(prog->last_error_msg), fmt, ap);
  return -1;
}

int fh_add_c_func(struct fh_program *prog, const char *name, fh_c_func func)
{
  return fh_compiler_add_c_func(&prog->compiler, name, func);
}

int fh_compile_file(struct fh_program *prog, const char *filename)
{
  struct fh_input *in = NULL;
  struct fh_ast *ast = NULL;

  //printf("------------------------\n");
  //printf("-> opening file '%s'...\n", filename);
  in = fh_open_input_file(filename);
  if (! in) {
    fh_set_error(prog, "ERROR: can't open '%s'", filename);
    goto err;
  }
  
  // parse
  ast = fh_new_ast();
  if (! ast) {
    fh_set_error(prog, "ERROR: out of memory for AST");
    goto err;
  }
  //printf("-> parsing...\n");
  if (fh_parse(&prog->parser, ast, in) < 0)
    goto err;
  //fh_dump_ast(ast);

  // compile
  //printf("-> compiling...\n");
  if (fh_compile(&prog->compiler, &prog->bc, ast) < 0)
    goto err;
  //printf("-> ok\n");
  //fh_dump_bc(&prog->bc);

  fh_free_ast(ast);
  fh_close_input(in);
  return 0;

 err:
  if (ast)
    fh_free_ast(ast);
  if (in)
    fh_close_input(in);
  return -1;
}

int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret)
{
  return fh_call_vm_function(&prog->vm, func_name, args, n_args, ret);
}

int fh_run_function(struct fh_program *prog, const char *func_name)
{
  struct fh_value ret;
  return fh_call_vm_function(&prog->vm, func_name, NULL, 0, &ret);
}

