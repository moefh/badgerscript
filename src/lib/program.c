/* fh.c */

#include <stdlib.h>
#include <stdio.h>

#include "program.h"
#include "c_funcs.h"

#define DEF_FN(name)  { #name, fh_fn_##name }

static const struct fh_named_c_func c_funcs[] = {
  DEF_FN(print),
  DEF_FN(printf),
  DEF_FN(len),
};

struct fh_program *fh_new_program(void)
{
  struct fh_program *prog = malloc(sizeof(struct fh_program));
  if (! prog)
    return NULL;
  prog->objects = NULL;
  prog->null_value.type = FH_VAL_NULL;
  prog->last_error_msg[0] = '\0';
  fh_init_stack(&prog->funcs, sizeof(struct fh_func *));

  fh_init_vm(&prog->vm, prog);
  fh_init_parser(&prog->parser, prog);
  fh_init_compiler(&prog->compiler, prog);
  fh_init_stack(&prog->c_vals, sizeof(struct fh_value));

  if (fh_add_c_funcs(prog, c_funcs, ARRAY_SIZE(c_funcs)) < 0)
    goto err;

  return prog;

 err:
  fh_free_stack(&prog->funcs);
  fh_free_stack(&prog->c_vals);
  fh_destroy_compiler(&prog->compiler);
  fh_destroy_parser(&prog->parser);  
  free(prog);
  return NULL;
}

void fh_free_program(struct fh_program *prog)
{
  fh_free_stack(&prog->funcs);
  fh_free_stack(&prog->c_vals);
  fh_collect_garbage(prog);
  fh_free_program_objects(prog);
  
  fh_destroy_vm(&prog->vm);
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
  if (fh_compiler_add_c_func(&prog->compiler, name, func) < 0)
    return fh_set_error(prog, "out of memory");
  return 0;
}

int fh_add_c_funcs(struct fh_program *prog, const struct fh_named_c_func *funcs, int n_funcs)
{
  for (int i = 0; i < n_funcs; i++)
    if (fh_add_c_func(prog, funcs[i].name, funcs[i].func) < 0)
      return -1;
  return 0;
}

int fh_compile_file(struct fh_program *prog, const char *filename)
{
  struct fh_input *in = NULL;
  struct fh_ast *ast = NULL;

  in = fh_open_input_file(filename);
  if (! in) {
    fh_set_error(prog, "can't open '%s'", filename);
    goto err;
  }
  
  ast = fh_new_ast();
  if (! ast) {
    fh_set_error(prog, "out of memory for AST");
    goto err;
  }
  if (fh_parse(&prog->parser, ast, in) < 0)
    goto err;
  //fh_dump_ast(ast);

  if (fh_compile(&prog->compiler, ast) < 0)
    goto err;
  //fh_dump_bc(prog);

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

