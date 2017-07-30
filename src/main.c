/* main.c */

#include <stdio.h>

#include "lib/ast.h"

int main(int argc, char **argv)
{
  if (argc <= 1) {
    printf("USAGE: %s filename\n", argv[0]);
    return 1;
  }
  struct fh_input *in = fh_open_input_file(argv[1]);
  if (! in) {
    printf("ERROR: can't open '%s'\n", argv[1]);
    return 1;
  }

  struct fh_ast *ast = fh_new_ast();
  if (! ast) {
    fh_close_input(in);
    printf("ERROR: out of memory for AST\n");
    return 1;
  }
  struct fh_parser *p = fh_new_parser(in, ast);
  if (! p) {
    fh_close_input(in);
    fh_free_ast(ast);
    printf("ERROR: out of memory for parser\n");
    return 1;
  }

  if (fh_parse(p) < 0) {
    printf("ERROR: %s\n", fh_get_parser_error(p));
  } else {
    fh_parser_dump(p);
  }
  
  fh_free_parser(p);
  fh_free_ast(ast);
  fh_close_input(in);
  
  return 0;
}
