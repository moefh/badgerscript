/* bytecode.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bytecode.h"

struct fh_bc_func_info {
  fh_symbol_id name;
  struct fh_bc_func func;
};

struct fh_bc {
  uint32_t *instr;
  uint32_t num_instr;
  uint32_t cap_instr;
  struct fh_symtab *symtab;
  struct fh_stack funcs;
};

struct fh_bc *fh_new_bc(void)
{
  struct fh_bc *bc = malloc(sizeof(struct fh_bc));
  if (bc == NULL)
    return NULL;
  
  bc->instr = NULL;
  bc->num_instr = 0;
  bc->cap_instr = 0;
  bc->symtab = NULL;
  fh_init_stack(&bc->funcs, sizeof(struct fh_bc_func_info));

  bc->symtab = fh_new_symtab();
  if (! bc->symtab)
    goto err;
  
  return bc;

 err:
  fh_free_bc(bc);
  return NULL;
}

static void free_bc_const(struct fh_value *c)
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

static void free_bc_func(struct fh_bc_func *func)
{
  stack_foreach(struct fh_value *, c, &func->consts) {
    free_bc_const(c);
  }
  fh_free_stack(&func->consts);
}

void fh_free_bc(struct fh_bc *bc)
{
  stack_foreach(struct fh_bc_func_info *, f, &bc->funcs) {
    free_bc_func(&f->func);
  }
  fh_free_stack(&bc->funcs);
  if (bc->symtab)
    fh_free_symtab(bc->symtab);
  if (bc->instr)
    free(bc->instr);
  free(bc);
}

uint32_t *fh_add_bc_instr(struct fh_bc *bc, struct fh_src_loc loc, uint32_t instr)
{
  UNUSED(loc); // TODO: record source location
  if (bc->num_instr >= bc->cap_instr) {
    int32_t new_cap = (bc->cap_instr + 32 + 1) / 32 * 32;
    uint32_t *new_p = realloc(bc->instr, sizeof(uint32_t) * new_cap);
    if (! new_p)
      return NULL;
    bc->instr = new_p;
    bc->cap_instr = new_cap;
  }
  uint32_t pc = bc->num_instr;
  bc->instr[bc->num_instr++] = instr;
  return bc->instr + pc;
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
  fi->func.addr = 0;
  fi->func.n_opc = 0;
  fi->func.n_regs = 0;
  fh_init_stack(&fi->func.consts, sizeof(struct fh_value));
  return &fi->func;
}

uint32_t fh_get_bc_instruction(struct fh_bc *bc, uint32_t addr)
{
  if (addr >= bc->num_instr)
    return 0;
  return bc->instr[addr];
}

void fh_set_bc_instruction(struct fh_bc *bc, uint32_t addr, uint32_t instr)
{
  if (addr >= bc->num_instr)
    return;
  bc->instr[addr] = instr;
}

uint32_t *fh_get_bc_code(struct fh_bc *bc, uint32_t *size)
{
  if (size)
    *size = bc->num_instr;
  return bc->instr;
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

struct fh_value *fh_get_bc_func_consts(struct fh_bc_func *func)
{
  return fh_stack_item(&func->consts, 0);
}

struct fh_value *fh_get_bc_func_const(struct fh_bc_func *func, int num)
{
  return fh_stack_item(&func->consts, num);
}

int fh_get_bc_func_num_consts(struct fh_bc_func *func)
{
  return func->consts.num;
}

static struct fh_value *add_const(struct fh_bc_func *func, int *k)
{
  if (! fh_push(&func->consts, NULL))
    return NULL;
  *k = func->consts.num - 1;
  return fh_stack_top(&func->consts);
}

int fh_add_bc_const_number(struct fh_bc_func *func, double num)
{
  int k = 0;
  stack_foreach(struct fh_value *, c, &func->consts) {
    if (c->type == FH_VAL_NUMBER && c->data.num == num)
      return k;
    k++;
  }
  
  struct fh_value *c = add_const(func, &k);
  if (! c)
    return -1;
  c->type = FH_VAL_NUMBER;
  c->data.num = num;
  return k;
}

int fh_add_bc_const_string(struct fh_bc_func *func, const char *str)
{
  int k = 0;
  stack_foreach(struct fh_value *, c, &func->consts) {
    if (c->type == FH_VAL_STRING && strcmp(c->data.str, str) == 0)
      return k;
    k++;
  }

  struct fh_value *c = add_const(func, &k);
  if (! c)
    return -1;
  char *dup = malloc(strlen(str)+1);
  if (! dup) {
    fh_pop(&func->consts, NULL);
    return -1;
  }
  strcpy(dup, str);
  c->type = FH_VAL_STRING;
  c->data.str = dup;
  return k;
}

int fh_add_bc_const_func(struct fh_bc_func *func, struct fh_bc_func *f)
{
  int k = 0;
  stack_foreach(struct fh_value *, c, &func->consts) {
    if (c->type == FH_VAL_FUNC && c->data.func == f)
      return k;
    k++;
  }

  struct fh_value *c = add_const(func, &k);
  if (! c)
    return -1;
  c->type = FH_VAL_FUNC;
  c->data.func = f;
  return k;
}

int fh_add_bc_const_c_func(struct fh_bc_func *func, fh_c_func f)
{
  int k = 0;
  stack_foreach(struct fh_value *, c, &func->consts) {
    if (c->type == FH_VAL_C_FUNC && c->data.c_func == f)
      return k;
    k++;
  }

  struct fh_value *c = add_const(func, &k);
  if (! c)
    return -1;
  c->type = FH_VAL_C_FUNC;
  c->data.c_func = f;
  return k;
}
