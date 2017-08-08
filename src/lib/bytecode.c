/* bytecode.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bytecode.h"

struct fh_bc_func_info {
  fh_symbol_id name;
  struct fh_bc_func func;
};

int fh_init_bc(struct fh_bc *bc)
{
  bc->symtab = NULL;
  fh_init_stack(&bc->funcs, sizeof(struct fh_bc_func_info));

  bc->symtab = fh_new_symtab();
  if (! bc->symtab)
    goto err;
  
  return 0;

 err:
  fh_destroy_bc(bc);
  return -1;
}

static void destroy_bc_const(struct fh_value *c)
{
  switch (c->type) {
  case FH_VAL_NUMBER:
  case FH_VAL_FUNC:
  case FH_VAL_C_FUNC:
    return;

  case FH_VAL_STRING:
    free(c->data.str);
    return;
  }

  fprintf(stderr, "ERROR: invalid constant type: %d\n", c->type);
}

static void destroy_bc_func(struct fh_bc_func *func)
{
  if (func->consts) {
    for (int i = 0; i < func->n_consts; i++) {
      destroy_bc_const(&func->consts[i]);
    }
    free(func->consts);
  }
  if (func->code) {
    free(func->code);
  }
}

void fh_destroy_bc(struct fh_bc *bc)
{
  stack_foreach(struct fh_bc_func_info *, f, &bc->funcs) {
    destroy_bc_func(&f->func);
  }
  fh_free_stack(&bc->funcs);
  if (bc->symtab)
    fh_free_symtab(bc->symtab);
}

struct fh_bc_func *fh_add_bc_func(struct fh_bc *bc, struct fh_src_loc loc, const char *name, int n_params)
{
  UNUSED(loc); // TODO: record source location

  fh_symbol_id name_id = fh_add_symbol(bc->symtab, name);
  if (name_id < 0)
    return NULL;
  
  struct fh_bc_func_info *fi = fh_push(&bc->funcs, NULL);
  if (! fi)
    return NULL;
  fi->name = name_id;
  fi->func.n_params = n_params;
  fi->func.n_regs = 0;
  fi->func.code = NULL;
  fi->func.code_size = 0;
  fi->func.consts = NULL;
  fi->func.n_consts = 0;

  return &fi->func;
}

int fh_get_bc_num_funcs(struct fh_bc *bc)
{
  return bc->funcs.num;
}

struct fh_bc_func *fh_get_bc_func(struct fh_bc *bc, int num)
{
  struct fh_bc_func_info *fi = fh_stack_item(&bc->funcs, num);
  if (! fi)
    return NULL;
  return &fi->func;
}

struct fh_bc_func *fh_get_bc_func_by_name(struct fh_bc *bc, const char *name)
{
  fh_symbol_id name_id = fh_get_symbol_id(bc->symtab, name);
  if (name_id < 0)
    return NULL;
  
  stack_foreach(struct fh_bc_func_info *, fi, &bc->funcs) {
    if (fi->name == name_id)
      return &fi->func;
  }
  return NULL;
}

const char *fh_get_bc_func_name(struct fh_bc *bc, int num)
{
  struct fh_bc_func_info *fi = fh_stack_item(&bc->funcs, num);
  if (! fi)
    return NULL;
  return fh_get_symbol_name(bc->symtab, fi->name);
}
