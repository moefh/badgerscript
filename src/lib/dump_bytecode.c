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

static void dump_instr_ret(uint32_t instr)
{
  if (GET_INSTR_RA(instr) == 0) {
    printf("\n");
    return;
  }
  
  int b = GET_INSTR_RB(instr);
  if (b <= MAX_FUNC_REGS)
    printf("r%d\n", b);
  else
    printf("c[%d]\n", b - MAX_FUNC_REGS - 1);
}

static void dump_instr_jmp(uint32_t instr, int32_t addr)
{
  int n_close = GET_INSTR_RA(instr);
  if (n_close)
    printf("<%d> %d\n", n_close, addr + 1 + GET_INSTR_RS(instr));
  else
    printf("%d\n", addr + 1 + GET_INSTR_RS(instr));
}

static void dump_instr_up_rkb(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  printf("u[%d], ", a);
  if (b <= MAX_FUNC_REGS)
    printf("r%d", b);
  else
    printf("c[%d]", b-MAX_FUNC_REGS-1);
  printf("\n");
}

static void dump_instr_ra_up(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  printf("r%d, ", a);
  printf("u[%d]\n", b);
}

static void dump_instr_abc(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);
  int c = GET_INSTR_RC(instr);

  printf("%d, %d, %d\n", a, b, c);
}

static void dump_instr_a_rkb(uint32_t instr)
{
  int a = GET_INSTR_RA(instr);
  int b = GET_INSTR_RB(instr);

  printf("%d, ", a);
  if (b <= MAX_FUNC_REGS)
    printf("r%d", b);
  else
    printf("c[%d]", b-MAX_FUNC_REGS-1);
  printf("\n");
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

  printf("r%d, ", a);
  printf("%d\n", b);
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
    printf("%5d       ", addr);
  else
    printf("     ");
  //printf("%08x     ", instr);
  enum fh_bc_opcode opc = GET_INSTR_OP(instr);
  switch (opc) {
  case OPC_RET:      printf("ret       "); dump_instr_ret(instr); return;
  case OPC_CALL:     printf("call      "); dump_instr_ra_b(instr); return;
  case OPC_CLOSURE:  printf("closure   "); dump_instr_ra_rkb(instr); return;
  case OPC_GETUPVAL: printf("getupval  "); dump_instr_ra_up(instr); return;
  case OPC_SETUPVAL: printf("setupval  "); dump_instr_up_rkb(instr); return;

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
  case OPC_NEWMAP:   printf("newmap    "); dump_instr_ra_u(instr); return;
    
  case OPC_CMP_EQ:   printf("cmp.eq    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_CMP_LT:   printf("cmp.lt    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_CMP_LE:   printf("cmp.le    "); dump_instr_a_rkb_rkc(instr); return;
  case OPC_TEST:     printf("test      "); dump_instr_a_rkb(instr); return;
  case OPC_JMP:      printf("jmp       "); dump_instr_jmp(instr, addr); return;
    
  case OPC_LDNULL:   printf("ldnull    r%d\n",            GET_INSTR_RA(instr)); return;
  case OPC_LDC:      printf("ldc       r%d, c[%d]\n",     GET_INSTR_RA(instr), GET_INSTR_RU(instr)); return;
  }

  printf("???       "); dump_instr_abc(instr); return;
}

static void dump_const(struct fh_program *prog, struct fh_value *c)
{
  switch (c->type) {
  case FH_VAL_NULL:   printf("null\n"); return;
  case FH_VAL_BOOL:   printf("%s\n", (c->data.b) ? "true" : "false"); return;
  case FH_VAL_NUMBER: printf("%f\n", c->data.num); return;
  case FH_VAL_STRING: dump_string(fh_get_string(c)); printf("\n"); return;
  case FH_VAL_ARRAY:  printf("<array of length %d>\n", fh_get_array_len(c)); return;
  case FH_VAL_MAP:    printf("<map of length %d, capacity %d>\n", GET_OBJ_MAP(c->data.obj)->len, GET_OBJ_MAP(c->data.obj)->cap); return;
  case FH_VAL_UPVAL:  printf("<upval>\n"); return;

  case FH_VAL_CLOSURE:
    {
      struct fh_closure *closure = GET_OBJ_CLOSURE(c->data.obj);
      if (closure->func_def->name) {
        printf("<closure %p of %s>\n", (void *) closure, GET_OBJ_STRING_DATA(closure->func_def->name));
      } else {
        printf("<closure %p of function %p>\n", (void *) closure, (void *) closure->func_def);
      }
      return;
    }

  case FH_VAL_FUNC_DEF:
    {
      struct fh_func_def *func_def = GET_OBJ_FUNC_DEF(c->data.obj);
      if (func_def->name) {
        printf("<function %s>\n", GET_OBJ_STRING_DATA(func_def->name));
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

static void dump_func_def(struct fh_program *prog, struct fh_func_def *func_def)
{
  const char *func_name = fh_get_func_def_name(func_def);
  
  if (func_name)
    printf("; function %s(): %u parameters, %d regs\n", func_name, func_def->n_params, func_def->n_regs);
  else
    printf("; function at %p: %u parameters, %d regs\n", (void *) func_def, func_def->n_params, func_def->n_regs);

  struct fh_src_loc loc = fh_make_src_loc(0,0,0);
  const char *code_src_loc = func_def->code_src_loc;
  const char *code_src_loc_end = code_src_loc + func_def->code_src_loc_size;

  //for (int i = 0; i < func_def->code_src_loc_size; i++) printf("%02x ", (uint8_t) code_src_loc[i]); printf("\n");
  
  for (int i = 0; i < func_def->code_size; i++) {
    if (code_src_loc) {
      code_src_loc = fh_decode_src_loc(code_src_loc, code_src_loc_end - code_src_loc, &loc, 1);
      printf("<%d> %4d:%-4d     ", loc.file_id, loc.line, loc.col);
    } else {
      printf("                  ");
    }
    fh_dump_bc_instr(prog, i, func_def->code[i]);
  }

  if (func_def->n_consts) {
    printf("; %d constants:\n", func_def->n_consts);
    for (int i = 0; i < func_def->n_consts; i++) {
      printf("c[%d] = ", i);
      dump_const(prog, &func_def->consts[i]);
    }
  }

  if (func_def->n_upvals) {
    printf("; %d upvals:\n", func_def->n_upvals);
    for (int i = 0; i < func_def->n_upvals; i++) {
      struct fh_upval_def *ud = &func_def->upvals[i];
      printf("u[%d]: parent's ", i);
      if (ud->type == FH_UPVAL_TYPE_UPVAL)
        printf("u[%d]\n", ud->num);
      else
        printf("r%d\n", ud->num);
    }
  }
  
  printf("; ===================================================\n");

  // dump child function definitions
  for (int i = 0; i < func_def->n_consts; i++) {
    if (func_def->consts[i].type == FH_VAL_FUNC_DEF) {
      dump_func_def(prog, GET_VAL_FUNC_DEF(&func_def->consts[i]));
    }
  }
}

void fh_dump_bytecode(struct fh_program *prog)
{
  printf("; === BYTECODE ======================================\n");
  int n_funcs = fh_get_num_global_funcs(prog);
  for (int i = 0; i < n_funcs; i++) {
    struct fh_closure *closure = fh_get_global_func_by_index(prog, i);
    dump_func_def(prog, closure->func_def);
  }
}
