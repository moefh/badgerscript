/* dump_bytecode.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bytecode.h"

static void dump_string(struct fh_output *out, const char *str)
{
  fh_output(out, "\"");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': fh_output(out, "\\n"); break;
    case '\r': fh_output(out, "\\r"); break;
    case '\t': fh_output(out, "\\t"); break;
    case '\\': fh_output(out, "\\\\"); break;
    case '"': fh_output(out, "\\\""); break;
    default:
      if (*p < 32)
        fh_output(out, "\\x%02x", (unsigned char) *p);
      else
        fh_output(out, "%c", *p);
      break;
    }
  }
  fh_output(out, "\"");
}

static void dump_instr_abc(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  fh_output(out, "%d, %d, %d\n", a, b, c);
}

static void dump_instr_a_rkb_rkc(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  fh_output(out, "%d, ", a);
  if (b <= MAX_FUNC_REGS)
    fh_output(out, "r%d, ", b);
  else
    fh_output(out, "c[%d], ", b-MAX_FUNC_REGS-1);
  if (c <= MAX_FUNC_REGS)
    fh_output(out, "r%d", c);
  else
    fh_output(out, "c[%d]", c-MAX_FUNC_REGS-1);
  fh_output(out, "\n");
}

static void dump_instr_ra_rkb_rkc(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  fh_output(out, "r%d, ", a);
  if (b <= MAX_FUNC_REGS)
    fh_output(out, "r%d, ", b);
  else
    fh_output(out, "c[%d], ", b-MAX_FUNC_REGS-1);
  if (c <= MAX_FUNC_REGS)
    fh_output(out, "r%d", c);
  else
    fh_output(out, "c[%d]", c-MAX_FUNC_REGS-1);
  fh_output(out, "\n");
}

static void dump_instr_ra_rkb(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  fh_output(out, "r%d, ", a);
  if (b <= MAX_FUNC_REGS)
    fh_output(out, "r%d", b);
  else
    fh_output(out, "c[%d]", b-MAX_FUNC_REGS-1);
  fh_output(out, "\n");
}

static void dump_instr_ra_b(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  fh_output(out, "r%d, %d\n", a, b);
}

static void dump_instr_ra_u(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int u = GET_INSTR_RU(instr);

  fh_output(out, "r%d, %d\n", a, u);
}

void fh_dump_bc_instr(struct fh_program *prog, struct fh_output *out, int32_t addr, uint32_t instr)
{
  UNUSED(prog);

  if (addr >= 0)
    fh_output(out, "%-5d", addr);
  else
    fh_output(out, "     ");
  fh_output(out, "%08x     ", instr);
  enum fh_bc_opcode opc = GET_INSTR_OP(instr);
  switch (opc) {
  case OPC_RET:
    if (GET_INSTR_RB(instr) != 0)
      fh_output(out, "ret       r%d\n", GET_INSTR_RA(instr));
    else
      fh_output(out, "ret\n");
    return;
    
  case OPC_CALL:     fh_output(out, "call      r%d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;

  case OPC_ADD:      fh_output(out, "add       "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_SUB:      fh_output(out, "sub       "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_MUL:      fh_output(out, "mul       "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_DIV:      fh_output(out, "div       "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_MOD:      fh_output(out, "mod       "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_NEG:      fh_output(out, "neg       "); dump_instr_ra_rkb(out, instr); return;
  case OPC_MOV:      fh_output(out, "mov       "); dump_instr_ra_rkb(out, instr); return;
  case OPC_NOT:      fh_output(out, "not       "); dump_instr_ra_rkb(out, instr); return;

  case OPC_GETEL:    fh_output(out, "getel     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_SETEL:    fh_output(out, "setel     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_NEWARRAY: fh_output(out, "newarray  "); dump_instr_ra_u(out, instr); return;
    
  case OPC_CMP_EQ:   fh_output(out, "cmp.eq    "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_CMP_LT:   fh_output(out, "cmp.lt    "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_CMP_LE:   fh_output(out, "cmp.le    "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_TEST:     fh_output(out, "test      "); dump_instr_ra_b(out, instr); return;
  case OPC_JMP:      fh_output(out, "jmp       %-12d  ; to %d\n",  GET_INSTR_RS(instr), addr + 1 + GET_INSTR_RS(instr)); return;
    
  case OPC_LDNULL:   fh_output(out, "ldnull    r%d\n",            GET_INSTR_RA(instr)); return;
  case OPC_LDC:      fh_output(out, "ldc       r%d, c[%d]\n",     GET_INSTR_RA(instr), GET_INSTR_RU(instr)); return;
  }

  fh_output(out, "???       "); dump_instr_abc(out, instr); return;
}

static void dump_const(struct fh_program *prog, struct fh_value *c, struct fh_output *out)
{
  switch (c->type) {
  case FH_VAL_NULL:   fh_output(out, "NULL\n"); return;
  case FH_VAL_NUMBER: fh_output(out, "%f\n", c->data.num); return;
  case FH_VAL_STRING: dump_string(out, fh_get_string(c)); fh_output(out, "\n"); return;
  case FH_VAL_ARRAY:  fh_output(out, "<array of length %d>\n", fh_get_array_len(c)); return;

  case FH_VAL_FUNC:
    {
      struct fh_func *func = GET_OBJ_FUNC(c->data.obj);
      if (func->name) {
        fh_output(out, "<function %s>\n", GET_OBJ_STRING_DATA(func->name));
      } else {
        fh_output(out, "<function at %p>\n", c->data.obj);
      }
      return;
    }
    
  case FH_VAL_C_FUNC:
    {
      const char *name = fh_get_c_func_name(prog, c->data.c_func);
      if (! name)
        fh_output(out, "<C function>\n");
      else
        fh_output(out, "<C function %s>\n", name);
    }
    return;
  }

  fh_output(out, "<INVALID CONSTANT TYPE: %d>\n", c->type);
}

void fh_dump_bytecode(struct fh_program *prog)
{
  int n_funcs = fh_get_num_funcs(prog);

  struct fh_output *out = NULL;
  
  fh_output(out, "; === BYTECODE ======================================\n");
  for (int i = 0; i < n_funcs; i++) {
    struct fh_func *func = fh_get_func(prog, i);
    const char *func_name = fh_get_func_object_name(func);

    if (func_name)
      fh_output(out, "; function %s(): %u parameters, %d regs\n", func_name, func->n_params, func->n_regs);
    else
      fh_output(out, "; function at %p: %u parameters, %d regs\n", func, func->n_params, func->n_regs);

    for (int i = 0; i < func->code_size; i++)
      fh_dump_bc_instr(prog, out, i, func->code[i]);

    fh_output(out, "; %d constants:\n", func->n_consts);
    for (int j = 0; j < func->n_consts; j++) {
      fh_output(out, "c[%d] = ", j);
      dump_const(prog, &func->consts[j], out);
    }

    fh_output(out, "; ===================================================\n");
  }
}
