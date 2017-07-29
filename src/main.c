/* main.c */

#include <stdio.h>

#include "lib/ast.h"

int main(int argc, char **argv)
{
  struct fh_parser *p;
  struct fh_input in;

  if (argc <= 1) {
    printf("USAGE: %s filename\n", argv[0]);
    return 1;
  }
  if (fh_input_open_file(&in, argv[1]) < 0) {
    printf("ERROR: can't open '%s'\n", argv[1]);
    return 1;
  }
  
  p = fh_new_parser(&in);
  if (p == NULL) {
    printf("ERROR: out of memory for parser\n");
    return 1;
  }

  if (fh_parse(p) < 0) {
    printf("ERROR: %s\n", fh_get_parser_error(p));
  } else {
    fh_parser_dump(p);
  }
  
  fh_free_parser(p);
  fh_input_close_file(&in);
  
  return 0;
}
