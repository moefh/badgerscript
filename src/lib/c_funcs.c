/* c_funcs.c */

#include <stdlib.h>
#include <limits.h>
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
  case FH_VAL_ARRAY: printf("<array of length %d>", fh_get_array_len(val)); return;
  case FH_VAL_CLOSURE: printf("<closure %p>", val->data.obj); return;
  case FH_VAL_FUNC_DEF: printf("<func def %p>", val->data.obj); return;
  case FH_VAL_C_FUNC: printf("<C function %p>", val->data.c_func); return;
  }
  printf("<invalid value %d>", val->type);
}

static int check_n_args(struct fh_program *prog, const char *func_name, int n_expected, int n_received)
{
  if (n_expected >= 0 && n_received != n_expected)
    return fh_set_error(prog, "%s: expected %d argument(s), got %d", func_name, n_expected, n_received);
  if (n_received != INT_MIN && n_received < -n_expected)
    return fh_set_error(prog, "%s: expected at least %d argument(s), got %d", func_name, -n_expected, n_received);
  return 0;
}

int fh_fn_len(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "len()", 1, n_args))
    return -1;
  
  struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
  if (! arr)
    return fh_set_error(prog, "len(): argument must be an array");
  *ret = fh_new_number(arr->len);
  return 0;
}

int fh_fn_append(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "append()", -2, n_args))
    return -1;
  struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
  if (! arr)
    return fh_set_error(prog, "append(): argument 1 must be an array");
  struct fh_value *new_items = fh_grow_array_object(prog, arr, n_args-1);
  if (! new_items)
    return fh_set_error(prog, "out of memory");
  memcpy(new_items, args + 1, sizeof(struct fh_value) * (n_args-1));
  *ret = args[0];
  return 0;
}

int fh_fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;
  
  for (int i = 0; i < n_args; i++)
    print_value(&args[i]);
  *ret = fh_new_null();
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
  *ret = fh_new_null();
  return 0;
}

