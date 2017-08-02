/* dump_bytecode.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bytecode.h"

static void dump_func_header(struct fh_bc *bc, struct fh_output *out, struct fh_bc_func *func)
{
  fh_output(out, "; function with %u parameters, %d variables\n", func->n_params, func->n_vars);
}

static void dump_instr(struct fh_bc *bc, struct fh_output *out, int32_t addr, uint32_t instr)
{
  fh_output(out, "%-5d  %08x  ", addr, instr);
  switch (GET_INSTR_OP(instr)) {
  case OP_RET:   fh_output(out, "RETURN  %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;
  case OP_CALL:  fh_output(out, "CALL    %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;
  case OP_JMP:   fh_output(out, "JMP     %d    ; to %d\n", GET_INSTR_RsBx(instr), addr + 1 + GET_INSTR_RsBx(instr)); return;
  case OP_ADD:   fh_output(out, "ADD     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_SUB:   fh_output(out, "SUB     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_MUL:   fh_output(out, "MUL     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_DIV:   fh_output(out, "DIV     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  }

  fh_output(out, "??        %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr));
}

void fh_dump_bc(struct fh_bc *bc, struct fh_output *out)
{
  uint32_t n_instr;
  uint32_t *instr = fh_get_bc_instructions(bc, &n_instr);
  uint32_t n_funcs;
  struct fh_bc_func *funcs = fh_get_bc_funcs(bc, &n_funcs);

  fh_output(out, "; bytecode contains %d instructions:\n", n_instr);
  for (uint32_t i = 0; i < n_instr; i++) {
    for (uint32_t j = 0; j < n_funcs; j++) {
      if (funcs[j].pc == i) {
        dump_func_header(bc, out, &funcs[j]);
        break;
      }
    }
    dump_instr(bc, out, i, instr[i]);
  }
  fh_output(out, "; end of bytecode\n");
}
