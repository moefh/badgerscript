/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#define HAVE_TIOCGWINSZ

#ifdef HAVE_TIOCGWINSZ
#include <sys/ioctl.h>
#endif

static int fn_get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args);

static const struct fh_named_c_func c_funcs[] = {
  { "get_term_lines", fn_get_term_lines },
};

static int fn_get_term_lines(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)args;
  (void)n_args;

#ifdef HAVE_TIOCGWINSZ
  struct winsize term_size;

  if (ioctl(0, TIOCGWINSZ, &term_size) < 0)
    fh_make_number(prog, ret, 25.0);
  else
    fh_make_number(prog, ret, (double) term_size.ws_row);
#else
  fh_make_number(prog, ret, 25);
#endif
  return 0;
}

static int run_script(struct fh_program *prog, char *script_file, char **args, int n_args)
{
  if (fh_compile_file(prog, script_file) < 0)
    return -1;

  struct fh_value *script_args = malloc(sizeof(struct fh_value) * n_args);
  for (int i = 0; i < n_args; i++) {
    if (fh_make_string(prog, &script_args[i], args[i]) < 0)
      return -1;
  }
  struct fh_value script_ret;
  
  int ret = fh_call_function(prog, "main", script_args, n_args, &script_ret);
  
  free(script_args);
  return ret;
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
