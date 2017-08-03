/* dump_bytecode.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bytecode.h"

static void dump_string(struct fh_output *out, const uint8_t *str)
{
  fh_output(out, "\"");
  for (const uint8_t *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': fh_output(out, "\\n"); break;
    case '\r': fh_output(out, "\\r"); break;
    case '\t': fh_output(out, "\\t"); break;
    case '\\': fh_output(out, "\\\\"); break;
    case '"': fh_output(out, "\\\""); break;
    default:
      fh_output(out, "%c", *p);
      break;
    }
  }
  fh_output(out, "\"");
}

static void dump_func_header(struct fh_bc *bc, struct fh_output *out, struct fh_bc_func *func)
{
  fh_output(out, "; function with %u parameters, %d regs\n", func->n_params, func->n_regs);
}

static void dump_instr(struct fh_bc *bc, struct fh_output *out, int32_t addr, uint32_t instr)
{
  fh_output(out, "%-5d  %08x  ", addr, instr);
  switch (GET_INSTR_OP(instr)) {
  case OP_RET:   fh_output(out, "RETURN  %d, %d\n",     GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;
  case OP_CALL:  fh_output(out, "CALL    %d, %d\n",     GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;
  case OP_JMP:   fh_output(out, "JMP     %d    ; to %d\n", GET_INSTR_RsBx(instr), addr + 1 + GET_INSTR_RsBx(instr)); return;

  case OP_ADD:   fh_output(out, "ADD     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_SUB:   fh_output(out, "SUB     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_MUL:   fh_output(out, "MUL     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;
  case OP_DIV:   fh_output(out, "DIV     %d, %d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;

  case OP_MOV:   fh_output(out, "MOV     %d, %d\n",     GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;

  case OP_LOAD0: fh_output(out, "LOAD0   %d\n",         GET_INSTR_RA(instr)); return;
  case OP_LOADK: fh_output(out, "LOADK   %d, %d\n",     GET_INSTR_RA(instr), GET_INSTR_RBx(instr)); return;
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

  for (int i = 0; i < n_funcs; i++) {
    fh_output(out, "\n; constants for function %d\n", i);
    uint32_t n_consts;
    struct fh_bc_const *consts = fh_get_bc_func_consts(&funcs[i], &n_consts);
    for (uint32_t j = 0; j < n_consts; j++) {
      switch (consts[j].type) {
      case FH_BC_CONST_NUMBER:
        fh_output(out, "[%d] %f\n", j, consts[j].data.num);
        break;
        
      case FH_BC_CONST_STRING:
        fh_output(out, "[%d] ", j);
        dump_string(out, consts[j].data.str);
        fh_output(out, "\n");
        break;

      default:
        fh_output(out, "[%d] INVALID CONSTANT TYPE: %d\n", j, consts[j].type);
      }
    }
  }
  
  fh_output(out, "\n; end of bytecode\n");
}
