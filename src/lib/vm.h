/* vm.h */

#ifndef VM_H_FILE
#define VM_H_FILE

#include "fh_internal.h"
#include "stack.h"

struct fh_vm_call_frame {
  struct fh_closure *closure;
  int base;
  uint32_t *ret_addr;
};

DECLARE_STACK(call_frame_stack, struct fh_vm_call_frame);

struct fh_program;

struct fh_vm {
  struct fh_program *prog;
  struct fh_value *stack;
  int stack_size;
  struct fh_upval *open_upvals;
  struct call_frame_stack call_stack;
  uint32_t *pc;
  struct fh_src_loc last_error_loc;
  int last_error_addr;
  int last_error_frame_index;
};

void fh_init_vm(struct fh_vm *vm, struct fh_program *prog);
void fh_destroy_vm(struct fh_vm *vm);
int fh_call_vm_function(struct fh_vm *vm, struct fh_closure *closure, struct fh_value *args, int n_args, struct fh_value *ret);
int fh_run_vm(struct fh_vm *vm);

#endif /* VM_H_FILE */
