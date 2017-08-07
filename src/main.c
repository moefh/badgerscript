/* main.c */

#include <stdio.h>

#include "lib/fh_i.h"
#include "lib/ast.h"
#include "lib/bytecode.h"

static void print_val(const struct fh_value *val)
{
  switch (val->type) {
  case FH_VAL_NUMBER: printf("%g", val->data.num); return;
  case FH_VAL_STRING: printf("%s", val->data.str);  return;
  case FH_VAL_FUNC: printf("<func %p>", val->data.func); return;
  case FH_VAL_C_FUNC: printf("<C func %p>", val->data.c_func); return;
  }
  printf("<invalid value %d>", val->type);
}

static int my_printf(struct fh_vm *vm, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (n_args == 0 || args[0].type != FH_VAL_STRING)
    goto end;

  int next_arg = 1;
  for (char *c = args[0].data.str; *c != '\0'; c++) {
    if (*c != '%') {
      putchar_unlocked(*c);
      continue;
    }
    c++;
    if (*c == '%') {
      putchar_unlocked('%');
      continue;
    }
    if (next_arg >= n_args)
      return fh_vm_error(vm, "argument not supplied for '%%%c'", *c);
    
    switch (*c) {
    case 'd':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_vm_error(vm, "invalid argument type for '%%%c'", *c);
      printf("%lld", (long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'u':
    case 'x':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_vm_error(vm, "invalid argument type for '%%%c'", *c);
      printf((*c == 'u') ? "%llu" : "%llx", (unsigned long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'f':
    case 'g':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_vm_error(vm, "invalid argument type for '%%%c'", *c);
      printf((*c == 'f') ? "%f" : "%g", args[next_arg].data.num);
      break;
      
    case 's':
      if (args[next_arg].type != FH_VAL_STRING)
        return fh_vm_error(vm, "invalid argument type for '%%%c'", *c);
      printf("%s", args[next_arg].data.str);
      break;
      
    default:
      return fh_vm_error(vm, "invalid format specifier: '%%%c'", *c);
    }
    next_arg++;
  }
  
 end:
  fh_make_number(ret, 0);
  return 0;
}

static int my_print(struct fh_vm *vm, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)vm;

  for (int i = 0; i < n_args; i++)
    print_val(&args[i]);
  fh_make_number(ret, 0);
  return 0;
}

/*
 * Compile a file, adding to the given bytecode
 */
static int compile_file(struct fh_bc *bc, const char *filename)
{
  struct fh_input *in;
  struct fh_ast *ast = NULL;
  struct fh_parser *p = NULL;
  struct fh_compiler *c = NULL;

  printf("------------------------\n");
  printf("-> opening file '%s'...\n", filename);
  in = fh_open_input_file(filename);
  if (! in) {
    printf("ERROR: can't open '%s'\n", filename);
    goto err;
  }
  
  // parse
  ast = fh_new_ast();
  if (! ast) {
    printf("ERROR: out of memory for AST\n");
    goto err;
  }
  p = fh_new_parser();
  if (! p) {
    printf("ERROR: out of memory for parser\n");
    goto err;
  }
  printf("-> parsing...\n");
  if (fh_parse(p, ast, in) < 0) {
    printf("%s:%s\n", filename, fh_get_parser_error(p));
    goto err;
  }
  fh_dump_ast(ast);

  // compile
  c = fh_new_compiler();
  if (! c) {
    printf("ERROR: out of memory for compiler\n");
    goto err;
  }
  fh_compiler_add_c_func(c, "print", my_print);
  fh_compiler_add_c_func(c, "printf", my_printf);
  printf("-> compiling...\n");
  if (fh_compile(c, bc, ast) < 0) {
    printf("%s:%s\n", filename, fh_get_compiler_error(c));
    goto err;
  }
  printf("-> ok\n");
  
  fh_free_compiler(c);
  fh_free_parser(p);
  fh_free_ast(ast);
  fh_close_input(in);
  return 0;

 err:
  if (c)
    fh_free_compiler(c);
  if (p)
    fh_free_parser(p);
  if (ast)
    fh_free_ast(ast);
  if (in)
    fh_close_input(in);
  return -1;
}

/*
 * Call a function in the given bytecode
 */
static int run_function(struct fh_bc *bc, const char *func_name)
{
  printf("-> calling function '%s'...\n", func_name);
  struct fh_vm *vm = fh_new_vm(bc);
  if (! vm)
    goto err;
  struct fh_value ret;
  if (fh_call_function(vm, func_name, NULL, 0, &ret) < 0) {
    printf("ERROR: %s\n", fh_get_vm_error(vm));
    goto err;
  }
  printf("-> function returned ");
  fh_dump_value(&ret);
  printf("\n");

  fh_free_vm(vm);
  return 0;
  
 err:
  fh_free_vm(vm);
  return -1;
}

int main(int argc, char **argv)
{
  if (argc <= 1) {
    printf("USAGE: %s filename\n", argv[0]);
    return 1;
  }

  struct fh_bc *bc = fh_new_bc();
  if (! bc) {
    printf("ERROR: out of memory for bytecode\n");
    goto err;
  }

  for (int i = 1; i < argc; i++) {
    if (compile_file(bc, argv[i]) < 0)
      goto err;
  }
  
  printf("\nCOMPILED BYTECODE:\n");
  fh_dump_bc(bc, NULL);
  if (run_function(bc, "main") < 0)
    goto err;
  fh_free_bc(bc);
  return 0;
  
 err:
  fh_free_bc(bc);
  return 1;
}
