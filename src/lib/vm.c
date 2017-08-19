/* vm.c */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

#include "vm.h"
#include "program.h"
#include "bytecode.h"

void fh_init_vm(struct fh_vm *vm, struct fh_program *prog)
{
  vm->prog = prog;
  vm->stack = NULL;
  vm->stack_size = 0;
  vm->open_upvals = NULL;
  call_frame_stack_init(&vm->call_stack);
}

void fh_destroy_vm(struct fh_vm *vm)
{
  if (vm->stack)
    free(vm->stack);
  call_frame_stack_free(&vm->call_stack);
}

static int vm_error(struct fh_vm *vm, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fh_set_verror(vm->prog, fmt, ap);
  va_end(ap);
  return -1;
}

static int ensure_stack_size(struct fh_vm *vm, int size)
{
  if (vm->stack_size >= size)
    return 0;
  int new_size = (size + 1024 + 1) / 1024 * 1024;
  void *new_stack = realloc(vm->stack, new_size * sizeof(struct fh_value));
  if (! new_stack)
    return vm_error(vm, "out of memory");
  vm->stack = new_stack;
  vm->stack_size = new_size;
  return 0;
}

static struct fh_vm_call_frame *prepare_call(struct fh_vm *vm, struct fh_closure *closure, int ret_reg, int n_args)
{
  struct fh_func_def *func_def = closure->func_def;
  
  if (ensure_stack_size(vm, ret_reg + 1 + func_def->n_regs) < 0)
    return NULL;
  if (n_args < func_def->n_params)
    memset(vm->stack + ret_reg + 1 + n_args, 0, (func_def->n_params - n_args) * sizeof(struct fh_value));

  memset(vm->stack + ret_reg + 1 + func_def->n_params, 0, (func_def->n_regs - func_def->n_params) * sizeof(struct fh_value));
  
  struct fh_vm_call_frame *frame = call_frame_stack_push(&vm->call_stack, NULL);
  if (! frame) {
    vm_error(vm, "out of memory");
    return NULL;
  }
  frame->closure = closure;
  frame->base = ret_reg + 1;
  frame->ret_addr = NULL;
  return frame;
}

static struct fh_vm_call_frame *prepare_c_call(struct fh_vm *vm, int ret_reg, int n_args)
{
  if (ensure_stack_size(vm, ret_reg + 1 + n_args) < 0)
    return NULL;
  
  struct fh_vm_call_frame *frame = call_frame_stack_push(&vm->call_stack, NULL);
  if (! frame) {
    vm_error(vm, "out of memory");
    return NULL;
  }
  frame->closure = NULL;
  frame->base = ret_reg + 1;
  frame->ret_addr = NULL;
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
  struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
  struct fh_value *reg_base = vm->stack + frame->base;
  printf("--- base=%d, n_regs=%d\n", frame->base, frame->closure->func_def->n_regs);
  for (int i = 0; i < frame->closure->func_def->n_regs; i++) {
    printf("[%-3d] r%-2d = ", i+frame->base, i);
    dump_val("", &reg_base[i]);
  }
  printf("----------------------------\n");
}

int fh_call_vm_function(struct fh_vm *vm, struct fh_closure *closure, struct fh_value *args, int n_args, struct fh_value *ret)
{
  if (n_args > closure->func_def->n_params)
    n_args = closure->func_def->n_params;
  
  struct fh_vm_call_frame *prev_frame = call_frame_stack_top(&vm->call_stack);
  int ret_reg = (prev_frame) ? prev_frame->base + prev_frame->closure->func_def->n_regs : 0;
  if (ensure_stack_size(vm, ret_reg + n_args + 1) < 0)
    return -1;
  memset(&vm->stack[ret_reg], 0, sizeof(struct fh_value));
  if (args)
    memcpy(&vm->stack[ret_reg+1], args, n_args*sizeof(struct fh_value));

  if (n_args < closure->func_def->n_regs)
    memset(&vm->stack[ret_reg+1+n_args], 0, (closure->func_def->n_regs-n_args)*sizeof(struct fh_value));

  if (! prepare_call(vm, closure, ret_reg, n_args))
    return -1;
  vm->pc = closure->func_def->code;
  if (fh_run_vm(vm) < 0)
    return -1;
  if (ret)
    *ret = vm->stack[ret_reg];
  return 0;
}

static int call_c_func(struct fh_vm *vm, fh_c_func func, struct fh_value *ret, struct fh_value *args, int n_args)
{
  int num_c_vals = value_stack_size(&vm->prog->c_vals);
  
  int r = func(vm->prog, ret, args, n_args);

  value_stack_set_size(&vm->prog->c_vals, num_c_vals);  // release any objects created by the C function
  return r;
}

static struct fh_closure *new_closure(struct fh_vm *vm, struct fh_func_def *func_def)
{
  struct fh_closure *c = fh_make_closure(vm->prog, false);
  if (! c)
    return NULL;
  c->func_def = func_def;
  c->n_upvals = func_def->n_upvals;
  if (c->n_upvals > 0) {
    c->upvals = malloc(c->n_upvals * sizeof(struct fh_upval *));
    if (! c->upvals) {
      vm_error(vm, "out of memory");
      return NULL;
    }
  } else
    c->upvals = NULL;
  return c;
}

bool fh_val_is_true(struct fh_value *val)
{
  if (val->type == FH_VAL_UPVAL)
    val = GET_OBJ_UPVAL(val)->val;
  switch (val->type) {
  case FH_VAL_NULL:      return false;
  case FH_VAL_BOOL:      return val->data.b;
  case FH_VAL_NUMBER:    return val->data.num != 0.0;
  case FH_VAL_STRING:    return GET_VAL_STRING_DATA(val)[0] != '\0';
  case FH_VAL_ARRAY:     return true;
  case FH_VAL_MAP:       return true;
  case FH_VAL_CLOSURE:   return true;
  case FH_VAL_FUNC_DEF:  return true;
  case FH_VAL_C_FUNC:    return true;
  case FH_VAL_UPVAL:     return false;
  }
  return false;
}

bool fh_vals_are_equal(struct fh_value *v1, struct fh_value *v2)
{
  if (v1->type == FH_VAL_UPVAL)
    v1 = GET_OBJ_UPVAL(v1)->val;
  if (v2->type == FH_VAL_UPVAL)
    v2 = GET_OBJ_UPVAL(v2)->val;

  if (v1->type != v2->type)
    return false;
  switch (v1->type) {
  case FH_VAL_NULL:      return true;
  case FH_VAL_BOOL:      return v1->data.b == v2->data.b;
  case FH_VAL_NUMBER:    return v1->data.num == v2->data.num;
  case FH_VAL_C_FUNC:    return v1->data.c_func == v2->data.c_func;
  case FH_VAL_ARRAY:     return v1->data.obj == v2->data.obj;
  case FH_VAL_MAP:       return v1->data.obj == v2->data.obj;
  case FH_VAL_CLOSURE:   return v1->data.obj == v2->data.obj;
  case FH_VAL_FUNC_DEF:  return v1->data.obj == v2->data.obj;
  case FH_VAL_UPVAL:     return false;

  case FH_VAL_STRING:
    if (GET_VAL_STRING(v1)->hash != GET_VAL_STRING(v2)->hash)
      return false;
    return strcmp(GET_OBJ_STRING_DATA(v1->data.obj), GET_OBJ_STRING_DATA(v2->data.obj)) == 0;
  }
  return false;
}

static struct fh_upval *find_or_add_upval(struct fh_vm *vm, struct fh_value *val)
{
  struct fh_upval **cur = &vm->open_upvals;
  while (*cur != NULL && (*cur)->val >= val) {
    if ((*cur)->val == val)
      return *cur;
    cur = &(*cur)->data.next;
  }
  struct fh_upval *uv = fh_make_upval(vm->prog, false);
  uv->val = val;
  uv->data.next = *cur;
  *cur = uv;
  return uv;
}

static void close_upval(struct fh_vm *vm)
{
  struct fh_upval *uv = vm->open_upvals;
  //printf("CLOSING UPVAL %p (", (void *) uv); fh_dump_value(uv->val); printf(")\n");
  vm->open_upvals = uv->data.next;
  uv->data.storage = *uv->val;
  uv->val = &uv->data.storage;
}

static void dump_state(struct fh_vm *vm)
{
  struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
  printf("\n");
  printf("****************************\n");
  printf("***** HALTING ON ERROR *****\n");
  printf("****************************\n");
  printf("** current stack frame: ");
  if (frame) {
    if (frame->closure->func_def->name)
      printf("closure %p of %s\n", (void *) frame->closure, GET_OBJ_STRING_DATA(frame->closure->func_def->name));
    else
      printf("closure %p of function %p\n", (void *) frame->closure, (void *) frame->closure->func_def);
  } else
    printf("no stack frame!\n");
  dump_regs(vm);
  printf("** instruction that caused error:\n");
  int addr = (frame) ? vm->pc - 1 - frame->closure->func_def->code : -1;
  fh_dump_bc_instr(vm->prog, addr, vm->pc[-1]);
  printf("----------------------------\n");
}

#define handle_op(op) case op:
#define LOAD_REG_OR_CONST(index)  (((index) <= MAX_FUNC_REGS) ? &reg_base[index] : &const_base[(index)-MAX_FUNC_REGS-1])
#define LOAD_REG(index)    (&reg_base[index])
#define LOAD_CONST(index)  (&const_base[(index)-MAX_FUNC_REGS-1])

int fh_run_vm(struct fh_vm *vm)
{
  struct fh_value *const_base;
  struct fh_value *reg_base;

  uint32_t *pc = vm->pc;
  
 changed_stack_frame:
  {
    struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
    const_base = frame->closure->func_def->consts;
    reg_base = vm->stack + frame->base;
  }
  while (1) {
    //dump_regs(vm);
    //fh_dump_bc_instr(vm->prog, -1, *pc);
    
    uint32_t instr = *pc++;
    struct fh_value *ra = &reg_base[GET_INSTR_RA(instr)];
    switch (GET_INSTR_OP(instr)) {
      handle_op(OPC_LDC) {
        *ra = const_base[GET_INSTR_RU(instr)];
        break;
      }

      handle_op(OPC_LDNULL) {
        ra->type = FH_VAL_NULL;
        break;
      }

      handle_op(OPC_MOV) {
        *ra = *LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        break;
      }

      handle_op(OPC_RET) {
        struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
        if (GET_INSTR_RA(instr))
          vm->stack[frame->base-1] = *LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        else
          vm->stack[frame->base-1].type = FH_VAL_NULL;

        // close function upvalues
        while (vm->open_upvals != NULL && vm->open_upvals->val >= vm->stack + frame->base)
          close_upval(vm);
        
        uint32_t *ret_addr = frame->ret_addr;
        call_frame_stack_pop(&vm->call_stack, NULL);
        if (call_frame_stack_size(&vm->call_stack) == 0 || ! ret_addr) {
          vm->pc = pc;
          return 0;
        }
        pc = ret_addr;
        goto changed_stack_frame;
      }

      handle_op(OPC_GETEL) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type == FH_VAL_ARRAY) {
          if (rc->type != FH_VAL_NUMBER) {
            vm_error(vm, "invalid array access (non-numeric index)");
            goto user_err;
          }
          struct fh_value *val = fh_get_array_item(rb, (int) rc->data.num);
          if (! val) {
            vm_error(vm, "invalid array index");
            goto user_err;
          }
          *ra = *val;
          break;
        } else if (rb->type == FH_VAL_MAP) {
          if (fh_get_map_value(rb, rc, ra) < 0) {
            vm_error(vm, "key not in map");
            goto user_err;
          }
          break;
        }
        vm_error(vm, "invalid element access (non-container object)");
        goto user_err;
      }

      handle_op(OPC_SETEL) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (ra->type == FH_VAL_ARRAY) {
          if (rb->type != FH_VAL_NUMBER) {
            vm_error(vm, "invalid array access (non-numeric index)");
            goto user_err;
          }
          struct fh_value *val = fh_get_array_item(ra, (int) rb->data.num);
          if (! val) {
            vm_error(vm, "invalid array index");
            goto user_err;
          }
          *val = *rc;
          break;
        } else if (ra->type == FH_VAL_MAP) {
          if (fh_add_map_entry(vm->prog, ra, rb, rc) < 0)
            goto err;
          break;
        }
        vm_error(vm, "invalid element access (non-container object)");
        goto user_err;
      }

      handle_op(OPC_NEWARRAY) {
        int n_elems = GET_INSTR_RU(instr);

        struct fh_array *arr = fh_make_array(vm->prog, false);
        if (! arr)
          goto err;
        if (n_elems != 0) {
          GC_PIN_OBJ(arr);
          struct fh_value *first = fh_grow_array_object(vm->prog, arr, n_elems);
          if (! first) {
            GC_UNPIN_OBJ(arr);
            goto err;
          }
          GC_UNPIN_OBJ(arr);
          memcpy(first, ra + 1, n_elems*sizeof(struct fh_value));
        }
        ra->type = FH_VAL_ARRAY;
        ra->data.obj = arr;
        break;
      }
      
      handle_op(OPC_NEWMAP) {
        int n_elems = GET_INSTR_RU(instr);

        struct fh_map *map = fh_make_map(vm->prog, false);
        if (! map)
          goto err;
        fh_alloc_map_object_len(map, n_elems/2);
        if (n_elems != 0) {
          GC_PIN_OBJ(map);
          for (int i = 0; i < n_elems/2; i++) {
            struct fh_value *key = &ra[2*i+1];
            struct fh_value *val = &ra[2*i+2];
            if (key->type == FH_VAL_NULL) {
              GC_UNPIN_OBJ(map);
              return vm_error(vm, "can't create array with null key");
            }
            if (fh_add_map_object_entry(vm->prog, map, key, val) < 0) {
              GC_UNPIN_OBJ(map);
              goto err;
            }
          }
          GC_UNPIN_OBJ(map);
        }
        ra->type = FH_VAL_MAP;
        ra->data.obj = map;
        break;
      }
      
      handle_op(OPC_CLOSURE) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        if (rb->type != FH_VAL_FUNC_DEF) {
          vm_error(vm, "invalid value for closure (not a func_def)");
          goto err;
        }
        struct fh_func_def *func_def = GET_VAL_FUNC_DEF(rb);
        struct fh_closure *c = new_closure(vm, func_def);
        if (! c)
          goto err;
        GC_PIN_OBJ(c);
        struct fh_vm_call_frame *frame = NULL;
        for (int i = 0; i < func_def->n_upvals; i++) {
          if (func_def->upvals[i].type == FH_UPVAL_TYPE_UPVAL) {
            if (frame == NULL)
              frame = call_frame_stack_top(&vm->call_stack);
            c->upvals[i] = frame->closure->upvals[func_def->upvals[i].num];
          } else {
            c->upvals[i] = find_or_add_upval(vm, &reg_base[func_def->upvals[i].num]);
            GC_PIN_OBJ(c->upvals[i]);
          }
        }
        ra->type = FH_VAL_CLOSURE;
        ra->data.obj = c;
        for (int i = 0; i < func_def->n_upvals; i++)
          GC_UNPIN_OBJ(c->upvals[i]);
        GC_UNPIN_OBJ(c);
        break;
      }

      handle_op(OPC_GETUPVAL) {
        int b = GET_INSTR_RB(instr);
        struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
        *ra = *frame->closure->upvals[b]->val;
        break;
      }

      handle_op(OPC_SETUPVAL) {
        int a = GET_INSTR_RA(instr);
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
        *frame->closure->upvals[a]->val = *rb;
        break;
      }
      
      handle_op(OPC_ADD) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric values");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = rb->data.num + rc->data.num;
        break;
      }
      
      handle_op(OPC_SUB) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric values");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = rb->data.num - rc->data.num;
        break;
      }

      handle_op(OPC_MUL) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric values");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = rb->data.num * rc->data.num;
        break;
      }

      handle_op(OPC_DIV) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric values");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = rb->data.num / rc->data.num;
        break;
      }

      handle_op(OPC_MOD) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric values");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = fmod(rb->data.num, rc->data.num);
        break;
      }

      handle_op(OPC_NEG) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        if (rb->type != FH_VAL_NUMBER) {
          vm_error(vm, "arithmetic on non-numeric value");
          goto user_err;
        }
        ra->type = FH_VAL_NUMBER;
        ra->data.num = -rb->data.num;
        break;
      }

      handle_op(OPC_NOT) {
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        *ra = fh_new_bool(! fh_val_is_true(rb));
        break;
      }

      handle_op(OPC_CALL) {
        //dump_regs(vm);
        struct fh_vm_call_frame *frame = call_frame_stack_top(&vm->call_stack);
        if (ra->type == FH_VAL_CLOSURE) {
          uint32_t *func_addr = GET_OBJ_CLOSURE(ra->data.obj)->func_def->code;
          
          /*
           * WARNING: prepare_call() may move the stack, so don't trust reg_base
           * or ra after calling it -- jumping to changed_stack_frame fixes it.
           */
          struct fh_vm_call_frame *new_frame = prepare_call(vm, GET_OBJ_CLOSURE(ra->data.obj), frame->base + GET_INSTR_RA(instr), GET_INSTR_RB(instr));
          if (! new_frame)
            goto err;
          new_frame->ret_addr = pc;
          pc = func_addr;
          goto changed_stack_frame;
        }
        if (ra->type == FH_VAL_C_FUNC) {
          fh_c_func c_func = ra->data.c_func;
          
          /*
           * WARNING: above warning about prepare_call() also applies to prepare_c_call()
           */
          struct fh_vm_call_frame *new_frame = prepare_c_call(vm, frame->base + GET_INSTR_RA(instr), GET_INSTR_RB(instr));
          if (! new_frame)
            goto err;
          int ret = call_c_func(vm, c_func, vm->stack + new_frame->base - 1, vm->stack + new_frame->base, GET_INSTR_RB(instr));
          call_frame_stack_pop(&vm->call_stack, NULL);
          if (ret < 0)
            goto user_err;
          goto changed_stack_frame;
        }
        vm_error(vm, "call to non-function value");
        goto user_err;
      }
      
      handle_op(OPC_JMP) {
        int a = GET_INSTR_RA(instr);
        for (int i = 0; i < a; i++)
          close_upval(vm);
        pc += GET_INSTR_RS(instr);
        break;
      }
      
      handle_op(OPC_TEST) {
        int a = GET_INSTR_RA(instr);
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        int test = fh_val_is_true(rb) ^ a;
        if (test) {
          pc++;
          break;
        }
        pc += GET_INSTR_RS(*pc) + 1;
        break;
      }

      handle_op(OPC_CMP_EQ) {
        int inv = GET_INSTR_RA(instr);
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        int test = fh_vals_are_equal(rb, rc) ^ inv;
        if (test) {
          pc++;
          break;
        }
        pc += GET_INSTR_RS(*pc) + 1;
        break;
      }

      handle_op(OPC_CMP_LT) {
        int inv = GET_INSTR_RA(instr);
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "using < with non-numeric values");
          goto user_err;
        }
        int test = (rb->data.num < rc->data.num) ^ inv;
        //printf("(%f < %f) ^ %d ==> %d\n", ra->data.num, rb->data.num, c, test);
        if (test) {
          pc++;
          break;
        }
        pc += GET_INSTR_RS(*pc) + 1;
        break;
      }

      handle_op(OPC_CMP_LE) {
        int inv = GET_INSTR_RA(instr);
        struct fh_value *rb = LOAD_REG_OR_CONST(GET_INSTR_RB(instr));
        struct fh_value *rc = LOAD_REG_OR_CONST(GET_INSTR_RC(instr));
        if (rb->type != FH_VAL_NUMBER || rc->type != FH_VAL_NUMBER) {
          vm_error(vm, "using <= with non-numeric values");
          goto user_err;
        }
        int test = (rb->data.num <= rc->data.num) ^ inv;
        //printf("(%f <= %f) ^ %d ==> %d\n", ra->data.num, rb->data.num, c, test);
        if (test) {
          pc++;
          break;
        }
        pc += GET_INSTR_RS(*pc) + 1;
        break;
      }

    default:
      vm_error(vm, "unhandled opcode");
      goto err;
    }
  }

 err:
  vm->pc = pc;
  dump_state(vm);
  return -1;
  
 user_err:
  vm->pc = pc;
  //dump_state(vm);
  return -1;
}
