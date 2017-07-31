/* bytecode.c */

#include <stdlib.h>
#include <stdio.h>

#include "bytecode.h"

struct fh_bc {
  uint32_t *instr;
  uint32_t num_instr;
  uint32_t cap_instr;
  struct fh_bc_func *funcs;
  uint32_t num_funcs;
  uint32_t cap_funcs;
};

struct fh_bc *fh_new_bc(void)
{
  struct fh_bc *bc = malloc(sizeof(struct fh_bc));
  if (bc == NULL)
    return NULL;
  
  bc->instr = NULL;
  bc->num_instr = 0;
  bc->cap_instr = 0;

  bc->funcs = NULL;
  bc->num_funcs = 0;
  bc->cap_funcs = 0;

  return bc;
}

void fh_free_bc(struct fh_bc *bc)
{
  if (bc->instr)
    free(bc->instr);
  if (bc->funcs)
    free(bc->funcs);
  free(bc);
}

uint32_t *fh_bc_add_instr(struct fh_bc *bc, struct fh_src_loc loc, uint32_t instr)
{
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

struct fh_bc_func *fh_bc_add_func(struct fh_bc *bc, struct fh_src_loc loc, int n_params, int n_vars)
{
  if (bc->num_funcs >= bc->cap_funcs) {
    int32_t new_cap = (bc->cap_funcs + 32 + 1) / 32 * 32;
    struct fh_bc_func *new_p = realloc(bc->funcs, sizeof(struct fh_bc_func) * new_cap);
    if (! new_p)
      return NULL;
    bc->funcs = new_p;
    bc->cap_funcs = new_cap;
  }
  uint32_t pc = bc->num_instr;
  struct fh_bc_func *func = &bc->funcs[bc->num_funcs++];
  func->pc = pc;
  func->n_params = n_params;
  func->n_vars = n_vars;
  return func;
}

uint32_t *fh_get_bc_instructions(struct fh_bc *bc, uint32_t *num)
{
  *num = bc->num_instr;
  return bc->instr;
}

struct fh_bc_func *fh_get_bc_funcs(struct fh_bc *bc, uint32_t *num)
{
  *num = bc->num_funcs;
  return bc->funcs;
}
