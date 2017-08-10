/* c_funcs.c */

#include <stdlib.h>
#include <stdio.h>

#include "fh.h"
#include "c_funcs.h"
#include "value.h"

static void print_value(struct fh_value *val)
{
  switch (val->type) {
  case FH_VAL_NULL: printf("null"); return;
  case FH_VAL_NUMBER: printf("%g", val->data.num); return;
  case FH_VAL_STRING: printf("%s", fh_get_string(val));  return;
  case FH_VAL_ARRAY: printf("<array with %d elements>", fh_get_array_len(val)); return;
  case FH_VAL_FUNC: printf("<function %p>", val->data.obj); return;
  case FH_VAL_C_FUNC: printf("<C function %p>", val->data.c_func); return;
  }
  printf("<invalid value %d>", val->type);
}

int fh_fn_len(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (n_args != 1)
    return fh_set_error(prog, "len(): invalid number of arguments: %d", n_args);
  struct fh_array *arr = fh_get_array(&args[0]);
  if (! arr)
    return fh_set_error(prog, "len(): argument must be an array");
  *ret = fh_new_number(prog, arr->len);
  return 0;
}

int fh_fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  for (int i = 0; i < n_args; i++)
    print_value(&args[i]);
  *ret = fh_new_number(prog, 0);
  return 0;
}

int fh_fn_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (n_args == 0 || args[0].type != FH_VAL_STRING)
    goto end;

  int next_arg = 1;
  for (const char *c = fh_get_string(&args[0]); *c != '\0'; c++) {
    if (*c != '%') {
      putchar(*c);
      continue;
    }
    c++;
    if (*c == '%') {
      putchar('%');
      continue;
    }
    if (next_arg >= n_args)
      return fh_set_error(prog, "printf(): no argument supplied for '%%%c'", *c);
    
    switch (*c) {
    case 'd':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf("%lld", (long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'u':
    case 'x':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf((*c == 'u') ? "%llu" : "%llx", (unsigned long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'f':
    case 'g':
      if (args[next_arg].type != FH_VAL_NUMBER)
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf((*c == 'f') ? "%f" : "%g", args[next_arg].data.num);
      break;
      
    case 's':
      print_value(&args[next_arg]);
      break;
      
    default:
      return fh_set_error(prog, "printf(): invalid format specifier: '%%%c'", *c);
    }
    next_arg++;
  }
  
 end:
  *ret = fh_new_number(prog, 0);
  return 0;
}

