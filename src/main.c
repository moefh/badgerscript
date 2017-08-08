/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#define HAVE_TIOCGWINSZ

#ifdef HAVE_TIOCGWINSZ
#include <sys/ioctl.h>
#endif

static int fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);
static int fn_get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

static const struct fh_named_c_func c_funcs[] = {
  { "print", fn_print },
  { "get_term_lines", fn_get_term_lines },
};

static int fn_print(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)prog;

  for (int i = 0; i < n_args; i++) {
    switch (args[i].type) {
    case FH_VAL_NUMBER: printf("%g", args[i].data.num); break;
    case FH_VAL_STRING: printf("%s", args[i].data.str);  break;
    case FH_VAL_FUNC: printf("<func %p>", args[i].data.func); break;
    case FH_VAL_C_FUNC: printf("<C func %p>", args[i].data.c_func); break;
    default: printf("<invalid value %d>", args[i].type); break;
    }
  }
  fh_make_number(ret, 0);
  return 0;
}

static int fn_get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
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

static int run_script(struct fh_program *prog, char *script_file, char **args, int n_args)
{
  if (fh_compile_file(prog, script_file) < 0)
    return -1;

  struct fh_value *script_args = malloc(sizeof(struct fh_value) * n_args);
  for (int i = 0; i < n_args; i++) {
    fh_make_string(&script_args[i], args[i]);
  }
  struct fh_value script_ret;
  
  if (fh_call_function(prog, "main", script_args, n_args, &script_ret) < 0)
    return -1;

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

  if (fh_add_c_funcs(prog, c_funcs, sizeof(c_funcs)/sizeof(c_funcs[0])) < 0)
    goto err;
  
  if (run_script(prog, argv[1], &argv[2], argc-2) < 0)
    goto err;
  
  fh_free_program(prog);
  return 0;

 err:
  printf("ERROR: %s\n", fh_get_error(prog));
  fh_free_program(prog);
  return 1;
}
