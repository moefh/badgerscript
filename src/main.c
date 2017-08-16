/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#define HAVE_TIOCGWINSZ

#ifdef HAVE_TIOCGWINSZ
#include <sys/ioctl.h>
#endif

static int fn_gc(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)args;
  (void)n_args;

  fh_collect_garbage(prog);
  *ret = fh_new_number(0);
  return 0;
}

static int fn_add_garbage(struct fh_program *prog, struct fh_value *ret, struct fh_value *args, int n_args)
{
  (void)args;
  (void)n_args;

  static int i = 0;
  char str[256];
  snprintf(str, sizeof(str), "garbage %d", i++);
  fh_new_string(prog, str);
  
  *ret = fh_new_null();
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
    *ret = fh_new_number(25.0);
  else
    *ret = fh_new_number(term_size.ws_row);
#else
  *ret = fh_new_number(25);
#endif
  return 0;
}

static int run_script(struct fh_program *prog, int dump_bytecode, char *script_file, char **args, int n_args)
{
  if (fh_compile_file(prog, script_file) < 0)
    return -1;

  if (dump_bytecode)
    fh_dump_bytecode(prog);
  
  struct fh_value script_args = fh_new_array(prog);
  if (script_args.type == FH_VAL_NULL)
    return -1;
  struct fh_value *items = fh_grow_array(prog, &script_args, n_args+1);
  if (! items)
    return -1;
  items[0] = fh_new_string(prog, script_file);
  if (items[0].type == FH_VAL_NULL)
    return -1;
  for (int i = 0; i < n_args; i++) {
    items[i+1] = fh_new_string(prog, args[i]);
    if (items[i+1].type == FH_VAL_NULL)
      return -1;
  }
  struct fh_value script_ret;
  
  return fh_call_function(prog, "main", &script_args, 1, &script_ret);
}

static void print_usage(char *progname)
{
  printf("USAGE: %s [options] filename [args...]\n", progname);
  printf("\n");
  printf("options:\n");
  printf("\n");
  printf("  -h      show this help\n");
  printf("  -d      dump bytecode\n");
  printf("\n");
}

int main(int argc, char **argv)
{
  char *script_filename = NULL;
  char **script_args = NULL;
  int num_script_args = 0;
  int dump_bytecode = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      script_filename = argv[i];
      script_args = argv + i + 1;
      num_script_args = argc - i - 1;
      break;
    }
    switch (argv[i][1]) {
    case 'h':
      print_usage(argv[0]);
      return 0;

    case 'd':
      dump_bytecode = 1;
      break;

    default:
      printf("%s: unknown option '%s'\n", argv[0], argv[i]);
      exit(1);
    }
  }
  if (script_filename == NULL) {
    print_usage(argv[0]);
    return 0;
  }

  //for (int i = 0; i < 40; i++) printf("===\n");
  
  struct fh_program *prog = fh_new_program();
  if (! prog) {
    printf("ERROR: can't create program\n");
    return 1;
  }
  //fh_set_gc_frequency(prog, -1); // disable GC

  static const struct fh_named_c_func c_funcs[] = {
    { "get_term_lines", fn_get_term_lines },
    { "gc", fn_gc },
    { "add_garbage", fn_add_garbage },
  };
  if (fh_add_c_funcs(prog, c_funcs, sizeof(c_funcs)/sizeof(c_funcs[0])) < 0)
    goto err;
  
  if (run_script(prog, dump_bytecode, script_filename, script_args, num_script_args) < 0)
    goto err;
  
  fh_free_program(prog);
  return 0;

 err:
  printf("ERROR: %s\n", fh_get_error(prog));
  fh_free_program(prog);
  return 1;
}
