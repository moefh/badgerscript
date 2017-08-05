/* bytecode.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bytecode.h"

struct fh_bc {
  uint32_t *instr;
  uint32_t num_instr;
  uint32_t cap_instr;
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

  fh_init_stack(&bc->funcs, sizeof(struct fh_bc_func));
  return bc;
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
  if (bc->instr)
    free(bc->instr);
  stack_foreach(struct fh_bc_func *, f, &bc->funcs) {
    free_bc_func(f);
  }
  fh_free_stack(&bc->funcs);
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

struct fh_bc_func *fh_add_bc_func(struct fh_bc *bc, struct fh_src_loc loc, int n_params)
{
  UNUSED(loc); // TODO: record source location
  if (! fh_push(&bc->funcs, NULL))
    return NULL;
  struct fh_bc_func *func = fh_stack_top(&bc->funcs);
  func->n_params = n_params;
  func->addr = 0;
  func->n_opc = 0;
  func->n_regs = 0;
  fh_init_stack(&func->consts, sizeof(struct fh_value));
  return func;
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

uint32_t fh_get_bc_num_instructions(struct fh_bc *bc)
{
  return bc->num_instr;
}

uint32_t *fh_get_bc_instructions(struct fh_bc *bc, uint32_t *num)
{
  if (num)
    *num = bc->num_instr;
  return bc->instr;
}

struct fh_bc_func *fh_get_bc_func(struct fh_bc *bc, int num)
{
  return fh_stack_item(&bc->funcs, num);
}

int fh_get_bc_num_funcs(struct fh_bc *bc)
{
  return bc->funcs.num;
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
