/* bytecode.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bytecode.h"
#include "program.h"

struct fh_func *fh_add_bc_func(struct fh_program *prog, struct fh_src_loc loc, const char *name, int n_params)
{
  UNUSED(loc); // TODO: record source location

  struct fh_func *func = fh_make_func(prog);
  if (! func)
    return NULL;
  func->name = fh_make_string(prog, name);
  func->n_params = n_params;
  func->n_regs = 0;
  func->code = NULL;
  func->code_size = 0;
  func->consts = NULL;
  func->n_consts = 0;

  if (! fh_push(&prog->funcs, &func))
    return NULL;
  return func;
}

int fh_get_bc_num_funcs(struct fh_program *prog)
{
  return prog->funcs.num;
}

struct fh_func *fh_get_bc_func(struct fh_program *prog, int num)
{
  struct fh_func **pf = fh_stack_item(&prog->funcs, num);
  if (! pf)
    return NULL;
  return *pf;
}

struct fh_func *fh_get_bc_func_by_name(struct fh_program *prog, const char *name)
{
  stack_foreach(struct fh_func **, pf, &prog->funcs) {
    struct fh_func *func = *pf;
    if (func->name != NULL && strcmp(GET_OBJ_STRING_DATA(func->name), name) == 0)
      return func;
  }
  return NULL;
}

const char *fh_get_bc_func_name(struct fh_program *prog, int num)
{
  struct fh_func **pf = fh_stack_item(&prog->funcs, num);
  if (! pf)
    return NULL;
  struct fh_func *func = *pf;
  if (! func->name)
    return NULL;
  return GET_OBJ_STRING_DATA(func->name);
}
