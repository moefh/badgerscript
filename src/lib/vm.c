/* vm.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "bytecode.h"

enum fh_vm_value_type {
  VM_NUMBER,
  VM_STRING,
  VM_FUNC,
  VM_C_FUNC,
};

struct fh_vm_value {
  enum fh_vm_value_type type;
  union {
    double num;
    char *str;
    struct fh_bc_func *func;
    fh_c_func *c_func;
  } data;
};

struct fh_vm_call_frame {
  struct fh_bc_func *func;
  int reg_base;
  int ret_reg;       // return value to this register
  uint32_t ret_addr; // return to this address
};

struct fh_vm {
  struct fh_stack val_stack;
  struct fh_stack call_stack;
  uint32_t *pc;
  uint32_t *code;
  struct fh_bc *bc;
};

struct fh_vm *fh_new_vm(struct fh_bc *bc)
{
  struct fh_vm *vm = malloc(sizeof(struct fh_vm));
  if (! vm)
    return NULL;

  vm->bc = bc;
  fh_init_stack(&vm->val_stack, sizeof(struct fh_vm_value));
  fh_init_stack(&vm->call_stack, sizeof(struct fh_vm_call_frame));
  vm->code = fh_get_bc_instructions(bc, NULL);
  return vm;
}

void fh_free_vm(struct fh_vm *vm)
{
  fh_free_stack(&vm->val_stack);
  fh_free_stack(&vm->call_stack);
  free(vm);
}

static void load_const(struct fh_vm_value *r, const struct fh_bc_const *c)
{
  switch (c->type) {
  case FH_BC_CONST_NUMBER: r->type = VM_NUMBER; r->data.num = c->data.num; return;
  case FH_BC_CONST_STRING: r->type = VM_STRING; r->data.str = c->data.str; return;
  case FH_BC_CONST_FUNC:   r->type = VM_FUNC;   r->data.func = c->data.func; return;
  case FH_BC_CONST_C_FUNC: r->type = VM_C_FUNC; r->data.c_func = c->data.c_func; return;
  }

  fprintf(stderr, "ERROR: invalid const type: %d\n", c->type);
}

static void dump_val(struct fh_vm_value *val)
{
  switch (val->type) {
  case VM_NUMBER: printf("NUMBER(%f)\n", val->data.num); return;
  case VM_STRING: printf("STRING(%s)\n", val->data.str); return;
  case VM_FUNC: printf("FUNC(addr=%u)\n", val->data.func->addr); return;
  case VM_C_FUNC: printf("C_FUNC(%p)\n", val->data.c_func); return;
  }
  printf("INVALID_VALUE(type=%d)\n", val->type);
}

int fh_run_vm_func(struct fh_vm *vm, struct fh_bc_func *func)
{
  int ret_reg = vm->val_stack.num;
  if (fh_push(&vm->val_stack, NULL) < 0)
    return -1;

  if (fh_push(&vm->call_stack, NULL) < 0) {
    fh_pop(&vm->val_stack, NULL);
    return -1;
  }

  struct fh_vm_call_frame *frame = fh_stack_top(&vm->call_stack);
  frame->func = func;
  frame->reg_base = vm->val_stack.num;
  frame->ret_reg = ret_reg;
  frame->ret_addr = (uint32_t) -1;

  if (fh_stack_grow(&vm->val_stack, func->n_regs+1) < 0)
    return -1;
  
  vm->pc = &vm->code[func->addr];
  if (fh_run_vm(vm) < 0)
    return -1;

  printf("RETURNED VALUE: "); dump_val(fh_stack_item(&vm->val_stack, ret_reg));
  return 0;
}

#define handle_op(op) case op:

int fh_run_vm(struct fh_vm *vm)
{
  struct fh_vm_call_frame *frame;
  struct fh_bc_const *const_base;
  struct fh_vm_value *reg_base;

  uint32_t *pc = vm->pc;
  
 changed_stack_frame:
  frame = fh_stack_top(&vm->call_stack);
  if (! frame)
    return -1;
  const_base = fh_stack_item(&frame->func->consts, 0);
  reg_base = fh_stack_item(&vm->val_stack, frame->reg_base);
  while (1) {
    printf("\n\n--- stack size is %d, base is %d\n", vm->val_stack.num, frame->reg_base);
    for (int i = 0; i < vm->val_stack.num-frame->reg_base; i++) {
      printf("[%d] r%d = ", i+frame->reg_base, i); dump_val(&reg_base[i]);
    }
    printf("---\n");
    fh_dump_bc_instr(vm->bc, NULL, pc - vm->code, *pc);
    
    uint32_t instr = *pc++;
    struct fh_vm_value *ra = &reg_base[GET_INSTR_RA(instr)];
    switch (GET_INSTR_OP(instr)) {
      handle_op(OPC_LDC) {
        struct fh_bc_const *c = &const_base[GET_INSTR_RU(instr)];
        load_const(ra, c);
        //printf("ldc r%d, c[%d] = ", GET_INSTR_RA(instr), GET_INSTR_RU(instr)); dump_val(ra);
        break;      
      }

      handle_op(OPC_MOV) {
        *ra = reg_base[GET_INSTR_RB(instr)];
        break;
      }
      
      handle_op(OPC_RET) {
        int has_val = GET_INSTR_RB(instr);
        if (has_val) {
          struct fh_vm_value *val = fh_stack_item(&vm->val_stack, frame->ret_reg);
          *val = *ra;
        }
        pc = &vm->code[frame->ret_addr];
        fh_pop(&vm->call_stack, NULL);
        if (vm->call_stack.num == 0) {
          vm->pc = pc;
          return 0;
        }
        goto changed_stack_frame;
      }

      handle_op(OPC_ADD) {
        int b = GET_INSTR_RB(instr);
        int c = GET_INSTR_RC(instr);
        struct fh_vm_value vb, vc;
        if (b <= MAX_FUNC_REGS) vb = reg_base[b]; else load_const(&vb, &const_base[b-MAX_FUNC_REGS-1]);
        if (c <= MAX_FUNC_REGS) vc = reg_base[c]; else load_const(&vc, &const_base[c-MAX_FUNC_REGS-1]);
        printf("b="); dump_val(&vb);
        printf("c="); dump_val(&vc);
        if (vb.type != VM_NUMBER || vc.type != VM_NUMBER) {
          fprintf(stderr, "arithmetic on non-numeric values\n");
          return -1;
        }
        ra->type = VM_NUMBER;
        ra->data.num = vb.data.num + vc.data.num;
        break;
      }

      handle_op(OPC_CALL) {
        int func_reg = GET_INSTR_RB(instr);
        struct fh_vm_value *func = &reg_base[func_reg];
        printf("func reg is %d\n", func_reg);
        dump_val(func);
        if (func->type != VM_FUNC) {
          fprintf(stderr, "call to non-function object\n");
          return -1;
        }

        if (fh_push(&vm->call_stack, NULL) < 0)
          return -1;
        struct fh_vm_call_frame *new_frame = fh_stack_top(&vm->call_stack);
        new_frame->func = func->data.func;
        new_frame->reg_base = frame->reg_base + func_reg + 1;
        new_frame->ret_reg = frame->reg_base + GET_INSTR_RA(instr);
        new_frame->ret_addr = pc - vm->code;

        if (fh_stack_grow(&vm->val_stack, new_frame->func->n_regs) < 0)
          return -1;
        
        pc = vm->code + new_frame->func->addr;
        goto changed_stack_frame;
      }
      
    default:
      fprintf(stderr, "ERROR: unhandled opcode\n");
      return -1;
    }
  }
}
