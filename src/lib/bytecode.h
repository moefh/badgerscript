/* bytecode.h */

#ifndef BYTECODE_H_FILE
#define BYTECODE_H_FILE

#include <stdint.h>

#include "fh_i.h"

enum fh_bc_opcode {
  OP_RET,
  OP_CALL,

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

#define PLACE_INSTR_OP(op)     ((uint32_t)(op )&0x3f )
#define PLACE_INSTR_RA(ra)    (((uint32_t)(ra )&0xff )<<6)
#define PLACE_INSTR_RB(rb)    (((uint32_t)(rb )&0x1ff)<<14)
#define PLACE_INSTR_RC(rc)    (((uint32_t)(rc )&0x1ff)<<23)
#define PLACE_INSTR_RBx(rbx)  (((uint32_t)(rbx)&0x3ffff)<<14)
#define PLACE_INSTR_RsBx(rbx)  PLACE_INSTR_RBx((rbx)+(1<<17))

#define MAKE_INSTR_ABC(op, ra, rb, rc)  (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb) | PLACE_INSTR_RC(rc))
#define MAKE_INSTR_ABx(op, ra, rbx)     (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RBx(rbx))
#define MAKE_INSTR_AsBx(op, ra, rbx)    (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RsBx(rbx))

struct fh_bc_func {
  uint32_t pc;
  uint32_t n_params;
  uint32_t n_vars;
};

struct fh_bc;

struct fh_bc *fh_new_bc(void);
void fh_free_bc(struct fh_bc *bc);
struct fh_bc_func *fh_bc_add_func(struct fh_bc *bc, struct fh_src_loc loc, int n_params, int n_vars);
uint32_t *fh_bc_add_instr(struct fh_bc *bc, struct fh_src_loc loc, uint32_t instr);
uint32_t *fh_get_bc_instructions(struct fh_bc *c, uint32_t *num);
struct fh_bc_func *fh_get_bc_funcs(struct fh_bc *c, uint32_t *num);

#endif /* BYTECODE_H_FILE */
