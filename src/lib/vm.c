/* vm.c */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "bytecode.h"

struct fh_vm_call_frame {
  struct fh_bc_func *func;
  int base;
  uint32_t ret_addr;
};

struct fh_vm {
  struct fh_value *stack;
  int stack_size;
  struct fh_stack call_stack;
  uint32_t *pc;
  uint32_t *code;
  struct fh_bc *bc;
  char last_err_msg[256];
};

struct fh_vm *fh_new_vm(struct fh_bc *bc)
{
  struct fh_vm *vm = malloc(sizeof(struct fh_vm));
  if (! vm)
    return NULL;

  vm->bc = bc;
  vm->stack = NULL;
  vm->stack_size = 0;
  fh_init_stack(&vm->call_stack, sizeof(struct fh_vm_call_frame));
  vm->code = fh_get_bc_code(bc, NULL);
  return vm;
}

void fh_free_vm(struct fh_vm *vm)
{
  if (vm->stack)
    free(vm->stack);
  fh_free_stack(&vm->call_stack);
  free(vm);
}

int fh_vm_error(struct fh_vm *vm, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(vm->last_err_msg, sizeof(vm->last_err_msg), fmt, ap);
  va_end(ap);
  return -1;
}

const char *fh_get_vm_error(struct fh_vm *vm)
{
  return vm->last_err_msg;
}

static int ensure_stack_size(struct fh_vm *vm, int size)
{
  if (vm->stack_size >= size)
    return 0;
  int new_size = (size + 1024 + 1) / 1024 * 1024;
  //int new_size = (size + 4 + 1) / 4 * 4;
  void *new_stack = realloc(vm->stack, new_size * sizeof(struct fh_value));
  if (! new_stack)
    return fh_vm_error(vm, "out of memory");
#if 0
  if (new_stack != vm->stack) {
    printf("**********************************************************\n");
    printf("********** STACK MOVED ***********************************\n");
    printf("**********************************************************\n");
  }
#endif
  vm->stack = new_stack;
  vm->stack_size = new_size;
  return 0;
}

struct fh_vm_call_frame *prepare_call(struct fh_vm *vm, struct fh_bc_func *func, int ret_reg, int n_args)
{
  if (ensure_stack_size(vm, ret_reg + 1 + func->n_regs) < 0)
    return NULL;
  if (n_args < func->n_params)
    memset(vm->stack + ret_reg + 1 + n_args, 0, (func->n_params - n_args) * sizeof(struct fh_value));

  /*
   * Clear uninitialized registers. See comment [XXX]
   */
  memset(vm->stack + ret_reg + 1 + func->n_params, 0, (func->n_regs - func->n_params) * sizeof(struct fh_value));
  
  struct fh_vm_call_frame *frame = fh_push(&vm->call_stack, NULL);
  if (! frame) {
    fh_vm_error(vm, "out of memory");
    return NULL;
  }
  frame->func = func;
  frame->base = ret_reg + 1;
  frame->ret_addr = (uint32_t) -1;
  return frame;
}

struct fh_vm_call_frame *prepare_c_call(struct fh_vm *vm, int ret_reg, int n_args)
{
  if (ensure_stack_size(vm, ret_reg + 1 + n_args) < 0)
    return NULL;
  
  struct fh_vm_call_frame *frame = fh_push(&vm->call_stack, NULL);
  if (! frame) {
    fh_vm_error(vm, "out of memory");
    return NULL;
  }
  frame->func = NULL;
  frame->base = ret_reg + 1;
  frame->ret_addr = (uint32_t) -1;
  return frame;
}

static void dump_val(char *label, struct fh_value *val)
{
  printf("%s", label);
  fh_dump_value(val);
  printf("\n");
}

static void dump_regs(struct fh_vm *vm)
{
  struct fh_vm_call_frame *frame = fh_stack_top(&vm->call_stack);
  struct fh_value *reg_base = vm->stack + frame->base;
  printf("--- base=%d, n_regs=%d\n", frame->base, frame->func->n_regs);
  for (int i = 0; i < frame->func->n_regs; i++) {
    printf("[%-3d] r%-2d = ", i+frame->base, i);
    dump_val("", &reg_base[i]);
  }
  printf("---\n");
}

int fh_call_vm_func(struct fh_vm *vm, const char *name, struct fh_value *args, int n_args, struct fh_value *ret)
{
  struct fh_bc_func *func = fh_get_bc_func_by_name(vm->bc, name);
  if (! func)
    return fh_vm_error(vm, "function '%s' doesn't exist", name);
  
  if (n_args > func->n_params)
    n_args = func->n_params;
  
  struct fh_vm_call_frame *prev_frame = fh_stack_top(&vm->call_stack);
  int ret_reg = (prev_frame) ? prev_frame->base + prev_frame->func->n_regs : 0;
  if (ensure_stack_size(vm, ret_reg + n_args + 1) < 0)
    return -1;
  memset(&vm->stack[ret_reg], 0, sizeof(struct fh_value));
  if (args)
    memcpy(&vm->stack[ret_reg+1], args, n_args*sizeof(struct fh_value));

  /*
   * [XXX] Clear uninitialized registers.
   *
   * This is not strictly necessary (since well-behaved bytecode will
   * never use a register before writing to it), but when debugging
   * with dump_regs() we get complaints from valgrind if we don't do
   * it:
   */
  if (n_args < func->n_regs)
    memset(&vm->stack[ret_reg+1+n_args], 0, (func->n_regs-n_args)*sizeof(struct fh_value));

  if (! prepare_call(vm, func, ret_reg, n_args))
    return -1;
  vm->pc = &vm->code[func->addr];
  if (fh_run_vm(vm) < 0)
    return -1;
  if (ret)
    *ret = vm->stack[ret_reg];
  return 0;
}

#define handle_op(op) case op:
#define LOAD_REG_OR_CONST(index)  (((index) <= MAX_FUNC_REGS) ? &reg_base[index] : &const_base[(index)-MAX_FUNC_REGS-1])

int fh_run_vm(struct fh_vm *vm)
{
  struct fh_value *const_base;
  struct fh_value *reg_base;

  uint32_t *pc = vm->pc;
  
 changed_stack_frame:
  {
    struct fh_vm_call_frame *frame = fh_stack_top(&vm->call_stack);
    const_base = fh_stack_item(&frame->func->consts, 0);
    reg_base = vm->stack + frame->base;
  }
  while (1) {
    //dump_regs(vm);
    fh_dump_bc_instr(vm->bc, NULL, pc - vm->code, *pc);
    
    uint32_t instr = *pc++;
    struct fh_value *ra = &reg_base[GET_INSTR_RA(instr)];
    switch (GET_INSTR_OP(instr)) {
      handle_op(OPC_LDC) {
        *ra = const_base[GET_INSTR_RU(instr)];
        break;      
      }

      handle_op(OPC_MOV) {
        *ra = reg_base[GET_INSTR_RB(instr)];
        break;
      }
      
      handle_op(OPC_RET) {
        struct fh_vm_call_frame *frame = fh_stack_top(&vm->call_stack);
        int has_val = GET_INSTR_RB(instr);
        if (has_val)
          vm->stack[frame->base-1] = *ra;
        else
          memset(&vm->stack[frame->base-1], 0, sizeof(struct fh_value));
        uint32_t ret_addr = frame->ret_addr;
        fh_pop(&vm->call_stack, NULL);
        if (vm->call_stack.num == 0 || ret_addr == (uint32_t) -1) {
          vm->pc = pc;
          return 0;
        }
        pc = vm->code + ret_addr;
        goto changed_stack_frame;
      }

      handle_op(OPC_ADD) {
        struct fh_value *vb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *vc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (vb->type != FH_VAL_NUMBER || vc->type != FH_VAL_NUMBER) {
          fh_vm_error(vm, "arithmetic on non-numeric values");
          goto err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = vb->data.num + vc->data.num;
        break;
      }
      
      handle_op(OPC_SUB) {
        struct fh_value *vb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *vc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (vb->type != FH_VAL_NUMBER || vc->type != FH_VAL_NUMBER) {
          fh_vm_error(vm, "arithmetic on non-numeric values");
          goto err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = vb->data.num - vc->data.num;
        break;
      }

      handle_op(OPC_MUL) {
        struct fh_value *vb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *vc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (vb->type != FH_VAL_NUMBER || vc->type != FH_VAL_NUMBER) {
          fh_vm_error(vm, "arithmetic on non-numeric values");
          goto err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = vb->data.num * vc->data.num;
        break;
      }

      handle_op(OPC_DIV) {
        struct fh_value *vb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *vc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (vb->type != FH_VAL_NUMBER || vc->type != FH_VAL_NUMBER) {
          fh_vm_error(vm, "arithmetic on non-numeric values");
          goto err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = vb->data.num / vc->data.num;
        break;
      }

      handle_op(OPC_NEG) {
        struct fh_value *vb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        if (vb->type != FH_VAL_NUMBER) {
          fh_vm_error(vm, "arithmetic on non-numeric value");
          goto err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = -vb->data.num;
        break;
      }
      
      handle_op(OPC_CALL) {
        struct fh_vm_call_frame *frame = fh_stack_top(&vm->call_stack);
        if (ra->type == FH_VAL_FUNC) {
          uint32_t func_addr = ra->data.func->addr;
          
          /*
           * WARNING: prepare_c_call() may move the stack, so don't trust reg_base
           * or ra after calling it -- jumping to changed_stack_frame fixes it.
           */
          struct fh_vm_call_frame *new_frame = prepare_call(vm, ra->data.func, frame->base + GET_INSTR_RA(instr), GET_INSTR_RB(instr));
          if (! new_frame)
            goto err;
          new_frame->ret_addr = pc - vm->code;
          pc = &vm->code[func_addr];
          goto changed_stack_frame;
        }
        if (ra->type == FH_VAL_C_FUNC) {
          fh_c_func c_func = ra->data.c_func;
          
          /*
           * WARNING: prepare_c_call() may move the stack, so don't trust reg_base
           * or ra after calling it -- jumping to changed_stack_frame fixes it.
           */
          struct fh_vm_call_frame *new_frame = prepare_c_call(vm, frame->base + GET_INSTR_RA(instr), GET_INSTR_RB(instr));
          if (! new_frame)
            goto err;
          if (c_func(vm, vm->stack + new_frame->base - 1, vm->stack + new_frame->base, GET_INSTR_RB(instr)) < 0)
            goto err;
          fh_pop(&vm->call_stack, NULL);
          goto changed_stack_frame;
        }
        fh_vm_error(vm, "call to non-function value");
        goto err;
      }
      
    default:
      fh_vm_error(vm, "ERROR: unhandled opcode");
      goto err;
    }
  }

 err:
  pc--;
  printf("\n");
  printf("****************************\n");
  printf("***** HALTING ON ERROR *****\n");
  printf("****************************\n");
  dump_regs(vm);
  fh_dump_bc_instr(vm->bc, NULL, pc - vm->code, *pc);
  vm->pc = pc;
  return -1;
}
