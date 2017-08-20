/* fh.c */

#include <stdlib.h>
#include <stdio.h>

#include "program.h"

struct fh_program *fh_new_program(void)
{
  struct fh_program *prog = malloc(sizeof(struct fh_program));
  if (! prog)
    return NULL;
  prog->gc_frequency = 100;     // collect every `gc_frequency` object creations
  prog->n_created_objs_since_last_gc = 0;
  prog->objects = NULL;
  prog->null_value.type = FH_VAL_NULL;
  prog->last_error_msg[0] = '\0';
  fh_init_symtab(&prog->src_file_names);
  p_closure_stack_init(&prog->global_funcs);
  named_c_func_stack_init(&prog->c_funcs);

  fh_init_vm(&prog->vm, prog);
  fh_init_parser(&prog->parser, prog);
  fh_init_compiler(&prog->compiler, prog);
  value_stack_init(&prog->c_vals);
  p_object_stack_init(&prog->pinned_objs);

  if (fh_add_c_funcs(prog, fh_std_c_funcs, fh_std_c_funcs_len) < 0)
    goto err;

  return prog;

 err:
  fh_destroy_symtab(&prog->src_file_names);
  p_closure_stack_free(&prog->global_funcs);
  p_object_stack_free(&prog->pinned_objs);
  named_c_func_stack_free(&prog->c_funcs);
  value_stack_free(&prog->c_vals);
  fh_destroy_compiler(&prog->compiler);
  fh_destroy_parser(&prog->parser);  
  free(prog);
  return NULL;
}

void fh_free_program(struct fh_program *prog)
{
  if (p_object_stack_size(&prog->pinned_objs) > 0)
    fprintf(stderr, "*** WARNING: %d pinned object(s) on exit\n", p_object_stack_size(&prog->pinned_objs));

  fh_destroy_symtab(&prog->src_file_names);
  p_closure_stack_free(&prog->global_funcs);
  p_object_stack_free(&prog->pinned_objs);
  named_c_func_stack_free(&prog->c_funcs);
  value_stack_free(&prog->c_vals);
  fh_collect_garbage(prog);
  fh_free_program_objects(prog);
  
  fh_destroy_vm(&prog->vm);
  fh_destroy_compiler(&prog->compiler);
  fh_destroy_parser(&prog->parser);

  free(prog);
}

void fh_set_gc_frequency(struct fh_program *prog, int frequency)
{
  prog->gc_frequency = frequency;
}

const char *fh_get_error(struct fh_program *prog)
{
  if (prog->vm.last_error_addr < 0)
    return prog->last_error_msg;
  char tmp[512];
  struct fh_src_loc *loc = &prog->vm.last_error_loc;
  snprintf(tmp, sizeof(tmp), "%s:%d:%d: %s",
           fh_get_symbol_name(&prog->src_file_names, loc->file_id),
           loc->line, loc->col, prog->last_error_msg);
  size_t size = (sizeof(tmp) > sizeof(prog->last_error_msg)) ? sizeof(prog->last_error_msg) : sizeof(tmp);
  memcpy(prog->last_error_msg, tmp, size);
  return prog->last_error_msg;
}

int fh_set_error(struct fh_program *prog, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(prog->last_error_msg, sizeof(prog->last_error_msg), fmt, ap);
  va_end(ap);
  prog->vm.last_error_addr = -1;
  return -1;
}

int fh_set_verror(struct fh_program *prog, const char *fmt, va_list ap)
{
  vsnprintf(prog->last_error_msg, sizeof(prog->last_error_msg), fmt, ap);
  prog->vm.last_error_addr = -1;
  return -1;
}

int fh_get_pin_state(struct fh_program *prog)
{
  return p_object_stack_size(&prog->pinned_objs);
}

void fh_restore_pin_state(struct fh_program *prog, int state)
{
  if (state > p_object_stack_size(&prog->pinned_objs)) {
    fprintf(stderr, "ERROR: invalid pin state\n");
    return;
  }
  p_object_stack_set_size(&prog->pinned_objs, state);
}

int fh_add_c_func(struct fh_program *prog, const char *name, fh_c_func func)
{
  struct named_c_func *cf = named_c_func_stack_push(&prog->c_funcs, NULL);
  if (! cf)
    return fh_set_error(prog, "out of memory");
  cf->name = name;
  cf->func = func;
  return 0;
}

int fh_add_c_funcs(struct fh_program *prog, const struct fh_named_c_func *funcs, int n_funcs)
{
  for (int i = 0; i < n_funcs; i++)
    if (fh_add_c_func(prog, funcs[i].name, funcs[i].func) < 0)
      return -1;
  return 0;
}

const char *fh_get_c_func_name(struct fh_program *prog, fh_c_func func)
{
  stack_foreach(struct named_c_func, *, c_func, &prog->c_funcs) {
    if (c_func->func == func)
      return c_func->name;
  }
  return NULL;
}

fh_c_func fh_get_c_func_by_name(struct fh_program *prog, const char *name)
{
  stack_foreach(struct named_c_func, *, c_func, &prog->c_funcs) {
    if (strcmp(name, c_func->name) == 0)
      return c_func->func;
  }
  return NULL;
}

int fh_add_global_func(struct fh_program *prog, struct fh_closure *closure)
{
  stack_foreach(struct fh_closure *, *, pc, &prog->global_funcs) {
    struct fh_closure *cur_closure = *pc;
    if (closure->func_def->name != NULL && strcmp(GET_OBJ_STRING_DATA(closure->func_def->name), GET_OBJ_STRING_DATA(cur_closure->func_def->name)) == 0) {
      *pc = closure;  // replace with new function
      return 0;
    }
  }
  if (! p_closure_stack_push(&prog->global_funcs, &closure))
    return fh_set_error(prog, "out of memory");
  return 0;
}

int fh_get_num_global_funcs(struct fh_program *prog)
{
  return p_closure_stack_size(&prog->global_funcs);
}

struct fh_closure *fh_get_global_func_by_index(struct fh_program *prog, int index)
{
  struct fh_closure **pc = p_closure_stack_item(&prog->global_funcs, index);
  if (! pc)
    return NULL;
  return *pc;
}

struct fh_closure *fh_get_global_func_by_name(struct fh_program *prog, const char *name)
{
  stack_foreach(struct fh_closure *, *, pc, &prog->global_funcs) {
    struct fh_closure *closure = *pc;
    if (closure->func_def->name != NULL && strcmp(GET_OBJ_STRING_DATA(closure->func_def->name), name) == 0)
      return closure;
  }
  return NULL;
}

int fh_compile_input(struct fh_program *prog, struct fh_input *in)
{
  struct fh_ast *ast = fh_new_ast(&prog->src_file_names);
  if (! ast) {
    fh_close_input(in);
    fh_set_error(prog, "out of memory for AST");
    return -1;
  }
  if (fh_parse(&prog->parser, ast, in) < 0)
    goto err;
  //fh_dump_ast(ast);

  if (fh_compile(&prog->compiler, ast) < 0)
    goto err;

  fh_free_ast(ast);
  return 0;

 err:
  if (ast)
    fh_free_ast(ast);
  return -1;
}

int fh_compile_file(struct fh_program *prog, const char *filename)
{
  struct fh_input *in = fh_open_input_file(filename);
  if (! in) {
    fh_set_error(prog, "can't open '%s'", filename);
    return -1;
  }
  return fh_compile_input(prog, in);
}

int fh_call_function(struct fh_program *prog, const char *func_name, struct fh_value *args, int n_args, struct fh_value *ret)
{
  struct fh_closure *closure = fh_get_global_func_by_name(prog, func_name);
  if (! closure)
    return fh_set_error(prog, "function '%s' doesn't exist", func_name);
  return fh_call_vm_function(&prog->vm, closure, args, n_args, ret);
}
