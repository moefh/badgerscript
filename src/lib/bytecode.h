/* bytecode.h */

#ifndef BYTECODE_H_FILE
#define BYTECODE_H_FILE

#include <stdint.h>

#include "fh_i.h"

enum fh_bc_opcode {
  OP_RET,
  OP_CALL,

  OP_MOV,
  OP_LOAD0,
  OP_LOADK,
  
  OP_JMP,
  
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
};

#define GET_INSTR_OP(instr)    (((uint32_t)(instr))&0x3f)
#define GET_INSTR_RA(instr)   ((((uint32_t)(instr))>>6)&0xff)
#define GET_INSTR_RB(instr)   ((((uint32_t)(instr))>>14)&0x1ff)
#define GET_INSTR_RC(instr)   ((((uint32_t)(instr))>>23)&0x1ff)
#define GET_INSTR_RBx(instr)  ((((uint32_t)(instr))>>14)&0x3ffff)
#define GET_INSTR_RsBx(instr) (((int32_t)GET_INSTR_RBx(instr))-(1<<17))

#define PLACE_INSTR_OP(op)     ((uint32_t)(op)&0x3f )
#define PLACE_INSTR_RA(ra)    (((uint32_t)(ra)&0xff )<<6)
#define PLACE_INSTR_RB(rb)    (((uint32_t)(rb)&0x1ff)<<14)
#define PLACE_INSTR_RC(rc)    (((uint32_t)(rc)&0x1ff)<<23)
#define PLACE_INSTR_RBx(rbx)  (((uint32_t)(rbx)&0x3ffff)<<14)
#define PLACE_INSTR_RsBx(rbx)  PLACE_INSTR_RBx((rbx)+(1<<17))

#define MAKE_INSTR_A(op, ra)            (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra))
#define MAKE_INSTR_AB(op, ra, rb)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb))
#define MAKE_INSTR_ABC(op, ra, rb, rc)  (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb) | PLACE_INSTR_RC(rc))
#define MAKE_INSTR_ABx(op, ra, rbx)     (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RBx(rbx))
#define MAKE_INSTR_AsBx(op, ra, rbx)    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RsBx(rbx))

enum fh_bc_const_type {
  FH_BC_CONST_NUMBER,
  FH_BC_CONST_STRING,
};

struct fh_bc_const {
  enum fh_bc_const_type type;
  union {
    double num;
    uint8_t *str;
  } data;
};

struct fh_bc_func {
  uint32_t pc;
  uint32_t n_params;
  uint32_t n_regs;
  struct fh_stack consts;
};

struct fh_bc;

struct fh_bc *fh_new_bc(void);
void fh_free_bc(struct fh_bc *bc);
struct fh_bc_func *fh_add_bc_func(struct fh_bc *bc, struct fh_src_loc loc, int n_params, int n_regs);
uint32_t *fh_add_bc_instr(struct fh_bc *bc, struct fh_src_loc loc, uint32_t instr);
int fh_add_bc_const_number(struct fh_bc_func *func, double num);
int fh_add_bc_const_string(struct fh_bc_func *func, const uint8_t *str);

uint32_t *fh_get_bc_instructions(struct fh_bc *c, uint32_t *num);
struct fh_bc_func *fh_get_bc_funcs(struct fh_bc *c, uint32_t *num);
struct fh_bc_const *fh_get_bc_func_consts(struct fh_bc_func *func, uint32_t *num);

#endif /* BYTECODE_H_FILE */
