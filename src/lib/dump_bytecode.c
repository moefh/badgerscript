/* dump_bytecode.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bytecode.h"

static void dump_string(const char *str)
{
  printf("\"");
  for (const char *p = str; *p != '\0'; p++) {
    switch (*p) {
    case '\n': printf("\\n"); break;
    case '\r': printf("\\r"); break;
    case '\t': printf("\\t"); break;
    case '\\': printf("\\\\"); break;
    case '"': printf("\\\""); break;
    default:
      if (*p < 32)
        printf("\\x%02x", (unsigned char) *p);
      else
        printf("%c", *p);
      break;
    }
  }
  printf("\"");
}

static void dump_instr_abc(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  printf("%d, %d, %d\n", a, b, c);
}

static void dump_instr_a_rkb_rkc(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  printf("%d, ", a);
  if (b <= MAX_FUNC_REGS)
    printf("r%d, ", b);
  else
    printf("c[%d], ", b-MAX_FUNC_REGS-1);
  if (c <= MAX_FUNC_REGS)
    printf("r%d", c);
  else
    printf("c[%d]", c-MAX_FUNC_REGS-1);
  printf("\n");
}

static void dump_instr_ra_rkb_rkc(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  printf("r%d, ", a);
  if (b <= MAX_FUNC_REGS)
    printf("r%d, ", b);
  else
    printf("c[%d], ", b-MAX_FUNC_REGS-1);
  if (c <= MAX_FUNC_REGS)
    printf("r%d", c);
  else
    printf("c[%d]", c-MAX_FUNC_REGS-1);
  printf("\n");
}

static void dump_instr_ra_rkb(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  printf("r%d, ", a);
  if (b <= MAX_FUNC_REGS)
    printf("r%d", b);
  else
    printf("c[%d]", b-MAX_FUNC_REGS-1);
  printf("\n");
}

static void dump_instr_ra_b(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  printf("r%d, %d\n", a, b);
}

static void dump_instr_ra_u(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int u = GET_INSTR_RU(instr);

  printf("r%d, %d\n", a, u);
}

void fh_dump_bc_instr(struct fh_program *prog, int32_t addr, uint32_t instr)
{
  UNUSED(prog);

  if (addr >= 0)
    printf("%-5d", addr);
  else
    printf("     ");
  printf("%08x     ", instr);
  enum fh_bc_opcode opc = GET_INSTR_OP(instr);
  switch (opc) {
  case OPC_RET:
    if (GET_INSTR_RA(instr) == 0)
      printf("ret\n");
    else {
      int b = GET_INSTR_RB(instr);
      if (b <= MAX_FUNC_REGS)
        printf("ret       r%d\n", b);
      else
        printf("ret       c[%d]\n", b - MAX_FUNC_REGS - 1);
    }
    return;
    
  case OPC_CALL:     printf("call      r%d, %d\n", GET_INSTR_RA(instr), GET_INSTR_RB(instr)); return;

  case OPC_ADD:      printf("add       "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_SUB:      printf("sub       "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_MUL:      printf("mul       "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_DIV:      printf("div       "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_MOD:      printf("mod       "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_NEG:      printf("neg       "); dump_instr_ra_rkb(instr); return;
  case OPC_MOV:      printf("mov       "); dump_instr_ra_rkb(instr); return;
  case OPC_NOT:      printf("not       "); dump_instr_ra_rkb(instr); return;

  case OPC_GETEL:    printf("getel     "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_SETEL:    printf("setel     "); dump_instr_ra_rkb_rkc(instr); return;
  case OPC_NEWARRAY: printf("newarray  "); dump_instr_ra_u(instr); return;
    
  case OPC_CMP_EQ:   printf("cmp.eq    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_CMP_LT:   printf("cmp.lt    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_CMP_LE:   printf("cmp.le    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_TEST:     printf("test      "); dump_instr_ra_b(instr); return;
  case OPC_JMP:      printf("jmp       %-12d  ; to %d\n",  GET_INSTR_RS(instr), addr + 1 + GET_INSTR_RS(instr)); return;
    
  case OPC_LDNULL:   printf("ldnull    r%d\n",            GET_INSTR_RA(instr)); return;
  case OPC_LDC:      printf("ldc       r%d, c[%d]\n",     GET_INSTR_RA(instr), GET_INSTR_RU(instr)); return;
  }

  printf("???       "); dump_instr_abc(instr); return;
}

static void dump_const(struct fh_program *prog, struct fh_value *c)
{
  switch (c->type) {
  case FH_VAL_NULL:   printf("NULL\n"); return;
  case FH_VAL_NUMBER: printf("%f\n", c->data.num); return;
  case FH_VAL_STRING: dump_string(fh_get_string(c)); printf("\n"); return;
  case FH_VAL_ARRAY:  printf("<array of length %d>\n", fh_get_array_len(c)); return;

  case FH_VAL_FUNC:
    {
      struct fh_func *func = GET_OBJ_FUNC(c->data.obj);
      if (func->name) {
        printf("<function %s>\n", GET_OBJ_STRING_DATA(func->name));
      } else {
        printf("<function at %p>\n", c->data.obj);
      }
      return;
    }
    
  case FH_VAL_C_FUNC:
    {
      const char *name = fh_get_c_func_name(prog, c->data.c_func);
      if (! name)
        printf("<C function>\n");
      else
        printf("<C function %s>\n", name);
    }
    return;
  }

  printf("<INVALID CONSTANT TYPE: %d>\n", c->type);
}

void fh_dump_bytecode(struct fh_program *prog)
{
  int n_funcs = fh_get_num_funcs(prog);

  printf("; === BYTECODE ======================================\n");
  for (int i = 0; i < n_funcs; i++) {
    struct fh_func *func = fh_get_func(prog, i);
    const char *func_name = fh_get_func_object_name(func);

    if (func_name)
      printf("; function %s(): %u parameters, %d regs\n", func_name, func->n_params, func->n_regs);
    else
      printf("; function at %p: %u parameters, %d regs\n", func, func->n_params, func->n_regs);

    for (int i = 0; i < func->code_size; i++)
      fh_dump_bc_instr(prog, i, func->code[i]);

    printf("; %d constants:\n", func->n_consts);
    for (int j = 0; j < func->n_consts; j++) {
      printf("c[%d] = ", j);
      dump_const(prog, &func->consts[j]);
    }

    printf("; ===================================================\n");
  }
}
