/* bytecode.h */

#ifndef BYTECODE_H_FILE
#define BYTECODE_H_FILE

#include <stdint.h>

#include "fh_i.h"

#define MAX_FUNC_REGS 256

enum fh_bc_opcode {
  OPC_RET,
  OPC_CALL,

  OPC_MOV,
  OPC_LD0,
  OPC_LDC,
  
  OPC_JMP,
  OPC_TEST,
  OPC_CMP_EQ,
  OPC_CMP_LT,
  OPC_CMP_LE,
  
  OPC_ADD,
  OPC_SUB,
  OPC_MUL,
  OPC_DIV,
  OPC_MOD,
  OPC_NEG,
};

#define GET_INSTR_OP(instr)    (((uint32_t)(instr))&0x3f)
#define GET_INSTR_RA(instr)   ((((uint32_t)(instr))>>6)&0xff)
#define GET_INSTR_RB(instr)   ((((uint32_t)(instr))>>14)&0x1ff)
#define GET_INSTR_RC(instr)   ((((uint32_t)(instr))>>23)&0x1ff)
#define GET_INSTR_RU(instr)   ((((uint32_t)(instr))>>14)&0x3ffff)
#define GET_INSTR_RS(instr)   (((int32_t)GET_INSTR_RU(instr))-(1<<17))

#define PLACE_INSTR_OP(op)     ((uint32_t)(op)&0x3f )
#define PLACE_INSTR_RA(ra)    (((uint32_t)(ra)&0xff )<<6)
#define PLACE_INSTR_RB(rb)    (((uint32_t)(rb)&0x1ff)<<14)
#define PLACE_INSTR_RC(rc)    (((uint32_t)(rc)&0x1ff)<<23)
#define PLACE_INSTR_RU(ru)    (((uint32_t)(ru)&0x3ffff)<<14)
#define PLACE_INSTR_RS(rs)    PLACE_INSTR_RU((rs)+(1<<17))

#define MAKE_INSTR_A(op, ra)            (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra))
#define MAKE_INSTR_AB(op, ra, rb)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb))
#define MAKE_INSTR_ABC(op, ra, rb, rc)  (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb) | PLACE_INSTR_RC(rc))
#define MAKE_INSTR_AU(op, ra, ru)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RU(ru))
#define MAKE_INSTR_AS(op, ra, rs)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RS(rs))

struct fh_bc;

struct fh_bc_func {
  uint32_t addr;
  int32_t n_params;
  int32_t n_opc;
  int32_t n_regs;
  struct fh_stack consts;
};

struct fh_bc *fh_new_bc(void);
void fh_free_bc(struct fh_bc *bc);
struct fh_bc_func *fh_add_bc_func(struct fh_bc *bc, struct fh_src_loc loc, const char *name, int n_params);
uint32_t *fh_add_bc_instr(struct fh_bc *bc, struct fh_src_loc loc, uint32_t instr);
int fh_add_bc_const_number(struct fh_bc_func *func, double num);
int fh_add_bc_const_string(struct fh_bc_func *func, const char *str);
int fh_add_bc_const_func(struct fh_bc_func *func, struct fh_bc_func *f);
int fh_add_bc_const_c_func(struct fh_bc_func *func, fh_c_func f);

uint32_t fh_get_bc_instruction(struct fh_bc *c, uint32_t addr);
void fh_set_bc_instruction(struct fh_bc *c, uint32_t addr, uint32_t instr);
uint32_t *fh_get_bc_code(struct fh_bc *c, uint32_t *size);

int fh_get_bc_num_funcs(struct fh_bc *bc);
struct fh_bc_func *fh_get_bc_func(struct fh_bc *bc, int num);
struct fh_bc_func *fh_get_bc_func_by_name(struct fh_bc *bc, const char *name);
const char *fh_get_bc_func_name(struct fh_bc *bc, int num);

struct fh_value *fh_get_bc_func_consts(struct fh_bc_func *func);
struct fh_value *fh_get_bc_func_const(struct fh_bc_func *func, int num);
int fh_get_bc_func_num_consts(struct fh_bc_func *func);

#endif /* BYTECODE_H_FILE */
