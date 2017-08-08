/* vm.h */

#ifndef VM_H_FILE
#define VM_H_FILE

#include "fh_i.h"
#include "stack.h"

struct fh_vm {
  struct fh_program *prog;
  struct fh_value *stack;
  int stack_size;
  struct fh_stack call_stack;
  uint32_t *pc;
  struct fh_bc *bc;
  char *last_err_msg;
};

void fh_init_vm(struct fh_vm *vm, struct fh_program *prog, struct fh_bc *bc);
void fh_destroy_vm(struct fh_vm *vm);
int fh_call_vm_function(struct fh_vm *vm, const char *name, struct fh_value *args, int n_args, struct fh_value *ret);
int fh_run_vm(struct fh_vm *vm);

#endif /* VM_H_FILE */
