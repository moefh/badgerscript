/* c_funcs.c */

#include <stdlib.h>
#include <stdio.h>

#include "fh.h"

int fh_printf(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
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
      if (args[next_arg].type != FH_VAL_STRING)
        return fh_set_error(prog, "printf(): invalid argument type for '%%%c'", *c);
      printf("%s", args[next_arg].data.str);
      break;
      
    default:
      return fh_set_error(prog, "printf(): invalid format specifier: '%%%c'", *c);
    }
    next_arg++;
  }
  
 end:
  fh_make_number(ret, 0);
  return 0;
}

