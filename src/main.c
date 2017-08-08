/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#define HAVE_TIOCGWINSZ

#ifdef HAVE_TIOCGWINSZ
#include <sys/ioctl.h>
#endif


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

static int my_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;

  for (int i = 0; i < n_args; i++)
    print_val(&args[i]);
  fh_make_number(ret, 0);
  return 0;
}

static int get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;
  (void)args;
  (void)n_args;

#ifdef HAVE_TIOCGWINSZ
  struct winsize term_size;

  if (ioctl(0, TIOCGWINSZ, &term_size) < 0)
    fh_make_number(ret, 25.0);
  else
    fh_make_number(ret, (double) term_size.ws_row);
#else
  fh_make_number(ret, 25);
#endif
  return 0;
}

int main(int argc, char **argv)
{
  if (argc <= 1) {
    printf("USAGE: %s filename\n", argv[0]);
    return 1;
  }

  struct fh_program *prog = fh_new_program();
  if (! prog) {
    printf("ERROR: can't create program\n");
    return 1;
  }

  if (fh_add_c_func(prog, "print", my_print) < 0)
    goto err;
  if (fh_add_c_func(prog, "get_term_lines", get_term_lines) < 0)
    goto err;
  
  for (int i = 1; i < argc; i++) {
    if (fh_compile_file(prog, argv[i]) < 0)
      goto err;
  }
  
  if (fh_run_function(prog, "main") < 0)
    goto err;

  fh_free_program(prog);
  return 0;

 err:
  printf("ERROR: %s\n", fh_get_error(prog));
  fh_free_program(prog);
  return 1;
}
