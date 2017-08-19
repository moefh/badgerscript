/* c_funcs.c */

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "fh.h"
#include "value.h"
#include "fh_internal.h"

static void print_value(struct fh_value *val)
{
  if (val->type == FH_VAL_UPVAL)
    val = GET_OBJ_UPVAL(val->data.obj)->val;
  
  switch (val->type) {
  case FH_VAL_NULL:      printf("null"); return;
  case FH_VAL_BOOL:      printf("%s", (val->data.b) ? "true" : "false"); return;
  case FH_VAL_NUMBER:    printf("%g", val->data.num); return;
  case FH_VAL_STRING:    printf("%s", GET_VAL_STRING_DATA(val)); return;
  case FH_VAL_ARRAY:     printf("<array of length %d>", GET_VAL_ARRAY(val)->len); return;
  case FH_VAL_MAP:       printf("<map of length %d>", GET_VAL_MAP(val)->len); return;
  case FH_VAL_CLOSURE:   printf("<closure %p>", val->data.obj); return;
  case FH_VAL_UPVAL:     printf("<internal error (upval)>"); return;
  case FH_VAL_FUNC_DEF:  printf("<func def %p>", val->data.obj); return;
  case FH_VAL_C_FUNC:
    {
      printf("<C function 0x");
      unsigned char *p = (unsigned char *) &val->data.c_func;
      p += sizeof(val->data.c_func);
      for (size_t i = 0; i < sizeof(val->data.c_func); i++)
        if (*--p)
          printf("%02x", *p);
      printf(">");
      return;
    }
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


static int fn_error(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)ret;
  
  if (check_n_args(prog, "error()", 1, n_args))
    return -1;

  const char *str = GET_VAL_STRING_DATA(&args[0]);
  if (! str)
    return fh_set_error(prog, "error(): argument 1 must be a string");
  return fh_set_error(prog, "%s", str);
}

static int fn_len(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "len()", 1, n_args))
    return -1;
  
  struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
  if (arr) {
    *ret = fh_make_number(arr->len);
    return 0;
  }
  struct fh_map *map = GET_VAL_MAP(&args[0]);
  if (map) {
    *ret = fh_make_number(map->len);
    return 0;
  }
  return fh_set_error(prog, "len(): argument 1 must be an array or map");
}

static int fn_delete(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "delete()", 2, n_args))
    return -1;

  struct fh_array *arr = GET_VAL_ARRAY(&args[0]);
  if (arr) {
    if (! fh_is_number(&args[1]))
      return fh_set_error(prog, "delete(): argument 2 must be a number");
    int index = (int) fh_get_number(&args[1]);
    if (index < 0 || index >= arr->len)
      return fh_set_error(prog, "delete(): array index out of bounds: %d", index);
    *ret = arr->items[index];
    if (index+1 < arr->len)
      memmove(arr->items + index, arr->items + index + 1, (arr->len - (index + 1)) * sizeof(struct fh_value));
    arr->len--;
    return 0;
  }
  struct fh_map *map = GET_VAL_MAP(&args[0]);
  if (map) {
    if (fh_get_map_object_value(map, &args[1], ret) < 0
        || fh_delete_map_object_entry(map, &args[1]) < 0)
      return fh_set_error(prog, "delete(): key not in map");
    return 0;
  }
  return fh_set_error(prog, "delete(): argument 1 must be an array or map");
}

static int fn_next_key(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "next_key()", 2, n_args))
    return -1;
  
  struct fh_map *map = GET_VAL_MAP(&args[0]);
  if (! map)
    return fh_set_error(prog, "next_key(): argument 1 must be a map");
  if (fh_next_map_object_key(map, &args[1], ret) < 0)
    return fh_set_error(prog, "next_key(): key not in map");
  return 0;
}

static int fn_contains_key(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (check_n_args(prog, "contains_key()", 2, n_args))
    return -1;
  
  struct fh_map *map = GET_VAL_MAP(&args[0]);
  if (! map)
    return fh_set_error(prog, "contains_key(): argument 1 must be a map");
  if (fh_get_map_object_value(map, &args[1], ret) < 0) {
    *ret = fh_make_bool(false);
  } else {
    printf("key "); print_value(&args[1]); printf(" has value "); print_value(ret); printf("\n");
    *ret = fh_make_bool(true);
  }
    
  return 0;
}

static int fn_append(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
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

static int fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;
  
  for (int i = 0; i < n_args; i++)
    print_value(&args[i]);
  *ret = fh_make_null();
  return 0;
}

static int fn_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  if (n_args == 0 || ! fh_is_string(&args[0]))
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
      if (! fh_is_number(&args[next_arg]))
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf("%lld", (long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'u':
    case 'x':
      if (! fh_is_number(&args[next_arg]))
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf((*c == 'u') ? "%llu" : "%llx", (unsigned long long) (int64_t) args[next_arg].data.num);
      break;
      
    case 'f':
    case 'g':
      if (! fh_is_number(&args[next_arg]))
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
  *ret = fh_make_null();
  return 0;
}

#define DEF_FN(name)  { #name, fn_##name }
const struct fh_named_c_func fh_std_c_funcs[] = {
  DEF_FN(error),
  DEF_FN(print),
  DEF_FN(printf),
  DEF_FN(len),
  DEF_FN(next_key),
  DEF_FN(contains_key),
  DEF_FN(append),
  DEF_FN(delete),
};
const int fh_std_c_funcs_len = sizeof(fh_std_c_funcs)/sizeof(fh_std_c_funcs[0]);
