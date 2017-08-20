/* main.c */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fh.h>

#include "functions.h"

static int run_string(struct fh_program *prog, bool dump_bytecode, const char *string)
{
  char *template = "function main(){%s;}";
  char *code = malloc(strlen(template) - 2 + strlen(string) + 1);
  if (! code) {
    fh_set_error(prog, "out of memory for string");
    return -1;
  }
  sprintf(code, template, string);

  struct fh_input *in = fh_open_input_string(code);
  if (! in) {
    free(code);
    fh_set_error(prog, "out of memory for string input");
    return -1;
  }
  free(code);
  
  if (add_functions(prog) < 0)
    return -1;

  if (fh_compile_input(prog, in) < 0)
    return -1;

  if (dump_bytecode)
    fh_dump_bytecode(prog);

  struct fh_value script_ret;
  if (fh_call_function(prog, "main", NULL, 0, &script_ret) < 0)
    return -1;
  
  if (fh_is_number(&script_ret))
    return (int) fh_get_number(&script_ret);
  return 0;
}

static int run_script_file(struct fh_program *prog, bool dump_bytecode, char *filename, char **args, int n_args)
{
  if (add_functions(prog) < 0)
    return -1;
  
  if (fh_compile_file(prog, filename) < 0)
    return -1;

  if (dump_bytecode)
    fh_dump_bytecode(prog);
  
  struct fh_value script_args = fh_new_array(prog);
  if (fh_is_null(&script_args))
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
  
  if (fh_is_number(&script_ret))
    return (int) fh_get_number(&script_ret);
  return 0;
}

static void print_usage(char *progname)
{
  printf("USAGE: %s [options] [filename [args...]]\n", progname);
  printf("\n");
  printf("options:\n");
  printf("\n");
  printf("  -e STRING    execute STRING\n");
  printf("  -d           dump bytecode before execution\n");
  printf("  -h           display this help\n");
  printf("\n");
  printf("Source code: <https://github.com/ricardo-massaro/badgerscript>\n");
}

int main(int argc, char **argv)
{
  char *execute_code = NULL;
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

    case 'e':
      execute_code = argv[++i];
      if (! execute_code) {
        printf("%s: option '-e' requires an argument\n", argv[0]);
        return 1;
      }
      break;

    default:
      printf("%s: unknown option '%s'\n", argv[0], argv[i]);
      return 1;
    }
  }
  if (! filename && ! execute_code) {
    print_usage(argv[0]);
    return 0;
  }

  struct fh_program *prog = fh_new_program();
  if (! prog) {
    printf("ERROR: out of memory for program\n");
    return 1;
  }

  int ret;
  if (execute_code)
    ret = run_string(prog, dump_bytecode, execute_code);
  else
    ret = run_script_file(prog, dump_bytecode, filename, args, num_args);
  if (ret < 0) {
    printf("ERROR: %s\n", fh_get_error(prog));
    ret = 1;
  }
  fh_free_program(prog);
  return ret;
}
