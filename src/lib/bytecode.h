/* bytecode.h */

#ifndef BYTECODE_H_FILE
#define BYTECODE_H_FILE

#include "fh_internal.h"
#include "value.h"

#define MAX_FUNC_REGS 256

enum fh_bc_opcode {
  OPC_RET,
  OPC_CALL,

  OPC_CLOSURE,
  OPC_GETUPVAL,
  OPC_SETUPVAL,
  
  OPC_MOV,
  OPC_LDNULL,
  OPC_LDC,
  
  OPC_JMP,
  OPC_TEST,
  OPC_CMP_EQ,
  OPC_CMP_LT,
  OPC_CMP_LE,

  OPC_GETEL,
  OPC_SETEL,
  OPC_NEWARRAY,
  OPC_NEWMAP,
  
  OPC_ADD,
  OPC_SUB,
  OPC_MUL,
  OPC_DIV,
  OPC_MOD,
  OPC_NEG,
  OPC_NOT,
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

#define INSTR_OP_MASK           0x3f
#define INSTR_RA_MASK         ( 0xff<<6)
#define INSTR_RB_MASK         (0x1ff<<14)
#define INSTR_RC_MASK         (0x1ff<<23)
#define INSTR_RU_MASK         ((uint32_t)0x3ffff<<14)
#define INSTR_RS_MASK         INSTR_RU_MASK

#define MAKE_INSTR_A(op, ra)            (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra))
#define MAKE_INSTR_AB(op, ra, rb)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb))
#define MAKE_INSTR_ABC(op, ra, rb, rc)  (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RB(rb) | PLACE_INSTR_RC(rc))
#define MAKE_INSTR_AU(op, ra, ru)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RU(ru))
#define MAKE_INSTR_AS(op, ra, rs)       (PLACE_INSTR_OP(op) | PLACE_INSTR_RA(ra) | PLACE_INSTR_RS(rs))

void fh_dump_bc_instr(struct fh_program *prog, int32_t addr, uint32_t instr);

#endif /* BYTECODE_H_FILE */
