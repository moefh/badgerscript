/* functions.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#include "functions.h"

#if defined (__linux__)
#include <sys/ioctl.h>
#elif defined (_WIN32)
#include <windows.h>
#endif

static int fn_get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;
  (void)args;
  (void)n_args;

#if defined (__linux__)
  struct winsize term_size;
  if (ioctl(0, TIOCGWINSZ, &term_size) >= 0) {
    *ret = fh_new_number(term_size.ws_row);
    return 0;
  }
#elif defined (_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) != 0
      || GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &csbi) != 0) {
    *ret = fh_new_number(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    return 0;
  }
#endif

  *ret = fh_new_number(25);
  return 0;
}

static int fn_gc(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)args;
  (void)n_args;

  fh_collect_garbage(prog);
  *ret = fh_new_null();
  return 0;
}

int add_functions(struct fh_program *prog)
{
  static const struct fh_named_c_func c_funcs[] = {
    { "get_term_lines",  fn_get_term_lines  },
    { "gc",              fn_gc              },
  };
  return fh_add_c_funcs(prog, c_funcs, sizeof(c_funcs)/sizeof(c_funcs[0]));
}
