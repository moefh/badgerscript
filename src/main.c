/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <fh.h>

#include "functions.h"

static int run_script_file(struct fh_program *prog, bool dump_bytecode, char *filename, char **args, int n_args)
{
  if (add_functions(prog) < 0)
    return -1;
  
  if (fh_compile_file(prog, filename) < 0)
    return -1;

  if (dump_bytecode)
    fh_dump_bytecode(prog);
  
  struct fh_value script_args = fh_new_array(prog);
  if (script_args.type == FH_VAL_NULL)
    return -1;
  struct fh_value *items = fh_grow_array(prog, &script_args, n_args+1);
  if (! items)
    return -1;
  items[0] = fh_new_string(prog, filename);
  for (int i = 0; i < n_args; i++)
    items[i+1] = fh_new_string(prog, args[i]);

  struct fh_value script_ret;
  if (fh_call_function(prog, "main", &script_args, 1, &script_ret) < 0)
    return -1;
  
  if (script_ret.type == FH_VAL_NUMBER)
    return (int) fh_get_number(&script_ret);
  return 0;
}

static void print_usage(char *progname)
{
  printf("USAGE: %s [options] filename [args...]\n", progname);
  printf("\n");
  printf("options:\n");
  printf("\n");
  printf("  -h     display this help\n");
  printf("  -d     dump bytecode before execution\n");
  printf("\n");
  printf("Source code: <https://github.com/ricardo-massaro/badgerscript>\n");
}

int main(int argc, char **argv)
{
  char *filename = NULL;
  char **args = NULL;
  int num_args = 0;
  bool dump_bytecode = false;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      filename = argv[i];
      args = argv + i + 1;
      num_args = argc - i - 1;
      break;
    }
    switch (argv[i][1]) {
    case 'h':
      print_usage(argv[0]);
      return 0;

    case 'd':
      dump_bytecode = true;
      break;

    default:
      printf("%s: unknown option '%s'\n", argv[0], argv[i]);
      return 1;
    }
  }
  if (filename == NULL) {
    print_usage(argv[0]);
    return 0;
  }

  struct fh_program *prog = fh_new_program();
  if (! prog) {
    printf("ERROR: out of memory for program\n");
    return 1;
  }

  int ret = run_script_file(prog, dump_bytecode, filename, args, num_args);
  if (ret < 0) {
    printf("ERROR: %s\n", fh_get_error(prog));
    ret = 1;
  }
  fh_free_program(prog);
  return ret;
}
