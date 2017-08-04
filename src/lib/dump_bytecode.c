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

static void dump_instr_a_rkb(struct fh_output *out, uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  fh_output(out, "%d, ", a);
  if (b <= MAX_FUNC_REGS)
    fh_output(out, "r%d", b);
  else
    fh_output(out, "c[%d]", b-MAX_FUNC_REGS-1);
  fh_output(out, "\n");
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

void fh_dump_bc_instr(struct fh_bc *bc, struct fh_output *out, int32_t addr, uint32_t instr)
{
  UNUSED(bc);
  
  fh_output(out, "%-5d  %08x     ", addr, instr);
  switch (GET_INSTR_OP(instr)) {
  case OPC_RET:
    if (GET_INSTR_RB(instr) != 0)
      fh_output(out, "ret     r%d\n", GET_INSTR_RA(instr));
    else
      fh_output(out, "ret\n");
    return;
    
  case OPC_CALL:    fh_output(out, "call    r%d, r%d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr), GET_INSTR_RC(instr)); return;

  case OPC_ADD:     fh_output(out, "add     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_SUB:     fh_output(out, "sub     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_MUL:     fh_output(out, "mul     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_DIV:     fh_output(out, "div     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_MOD:     fh_output(out, "mod     "); dump_instr_ra_rkb_rkc(out, instr); return;
  case OPC_MOV:     fh_output(out, "mov     "); dump_instr_ra_rkb(out, instr); return;

  case OPC_CMP_EQ:  fh_output(out, "cmp.eq  "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_CMP_LT:  fh_output(out, "cmp.lt  "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_CMP_LE:  fh_output(out, "cmp.le  "); dump_instr_a_rkb_rkc(out, instr); return;
  case OPC_TEST:    fh_output(out, "test    "); dump_instr_a_rkb(out, instr); return;
  case OPC_JMP:     fh_output(out, "jmp     %d            ; to %d\n",  GET_INSTR_RS(instr), addr + 1 + GET_INSTR_RS(instr)); return;
    
  case OPC_LD0:     fh_output(out, "ld0     r%d\n",            GET_INSTR_RA(instr)); return;
  case OPC_LDC:     fh_output(out, "ldc     r%d, c[%d]\n",     GET_INSTR_RA(instr), GET_INSTR_RU(instr)); return;
  }

  fh_output(out, "??      "); dump_instr_abc(out, instr); return;
}

static void dump_const(struct fh_bc_const *c, struct fh_output *out)
{
  switch (c->type) {
  case FH_BC_CONST_NUMBER:
    fh_output(out, "%f\n", c->data.num);
    return;
    
  case FH_BC_CONST_STRING:
    dump_string(out, c->data.str);
    fh_output(out, "\n");
    return;

  case FH_BC_CONST_FUNC:
    fh_output(out, "<function at %d>\n", c->data.func->addr);
    return;

  case FH_BC_CONST_C_FUNC:
    fh_output(out, "<C function at %p>\n", c->data.c_func);
    return;
  }

  fh_output(out, "<INVALID CONSTANT TYPE: %d>\n", c->type);
}

void fh_dump_bc(struct fh_bc *bc, struct fh_output *out)
{
  uint32_t n_instr;
  uint32_t *instr = fh_get_bc_instructions(bc, &n_instr);
  int n_funcs = fh_get_bc_num_funcs(bc);

  for (int i = 0; i < n_funcs; i++) {
    struct fh_bc_func *func = fh_get_bc_func(bc, i);

    fh_output(out, "; ===================================================\n");
    fh_output(out, "; function with %u parameters, %d regs\n", func->n_params, func->n_regs);
    
    for (uint32_t i = 0; i < func->n_opc; i++)
      fh_dump_bc_instr(bc, out, func->addr+i, instr[func->addr+i]);

    int n_consts = fh_get_bc_func_num_consts(func);
    fh_output(out, "\n; %d constants\n", n_consts);
    for (int j = 0; j < n_consts; j++) {
      struct fh_bc_const *c = fh_get_bc_func_const(func, j);
      fh_output(out, "c[%d] = ", j);
      dump_const(c, out);
    }

    fh_output(out, "\n");
  }
}
