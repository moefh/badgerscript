/* compiler.c */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "fh_i.h"
#include "ast.h"

struct fh_compiler {
  struct fh_ast *ast;
  uint8_t last_err_msg[256];
};

struct fh_compiler *fh_new_compiler(struct fh_ast *ast)
{
  struct fh_compiler *c = malloc(sizeof(struct fh_compiler));
  if (! c)
    return NULL;
  c->ast = ast;
  c->last_err_msg[0] = '\0';
  return c;
}

void fh_free_compiler(struct fh_compiler *c)
{
  free(c);
}

const uint8_t *fh_get_compiler_error(struct fh_compiler *c)
{
  return c->last_err_msg;
}

void *fh_compiler_error(struct fh_compiler *c, struct fh_src_loc loc, char *fmt, ...)
{
  char str[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(str, sizeof(str), fmt, ap);
  va_end(ap);

  snprintf((char *) c->last_err_msg, sizeof(c->last_err_msg), "%d:%d: %s", loc.line, loc.col, str);
  return NULL;
}

static int compile_named_func(struct fh_compiler *c, struct fh_p_named_func *func)
{
  fh_compiler_error(c, func->loc, "compilation not implemented");
  return -1;
}

int fh_compile(struct fh_compiler *c)
{
  for (int i = 0; i < c->ast->funcs.num; i++) {
    struct fh_p_named_func *f = fh_stack_item(&c->ast->funcs, i);
    if (compile_named_func(c, f) < 0)
      goto err;
  }
  return 0;

 err:
  return -1;
}
