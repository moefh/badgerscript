/* operator.h */

#include <stdlib.h>
#include <string.h>

#include "fh_internal.h"
#include "ast.h"

static struct fh_operator ops[] = {
  { '=',        "=",  FH_ASSOC_RIGHT,  10 },
  
  { AST_OP_OR,  "||", FH_ASSOC_LEFT,   20 },
  { AST_OP_AND, "&&", FH_ASSOC_LEFT,   30 },
  
  { '|',        "|",  FH_ASSOC_LEFT,   40 },
  { '&',        "&",  FH_ASSOC_LEFT,   50 },

  { AST_OP_EQ,  "==", FH_ASSOC_LEFT,   60 },
  { AST_OP_NEQ, "!=", FH_ASSOC_LEFT,   60 },
  { '<',        "<",  FH_ASSOC_LEFT,   70 },
  { '>',        ">",  FH_ASSOC_LEFT,   70 },
  { AST_OP_LE,  "<=", FH_ASSOC_LEFT,   70 },
  { AST_OP_GE,  ">=", FH_ASSOC_LEFT,   70 },

  { '+',        "+",  FH_ASSOC_LEFT,   80 },
  { '-',        "-",  FH_ASSOC_LEFT,   80 },
  { '*',        "*",  FH_ASSOC_LEFT,   90 },
  { '/',        "/",  FH_ASSOC_LEFT,   90 },
  { '%',        "%",  FH_ASSOC_LEFT,   90 },

  { AST_OP_UNM, "-",  FH_ASSOC_PREFIX, 100 },
  { '!',        "!",  FH_ASSOC_PREFIX, 100 },

  { '^',        "^",  FH_ASSOC_RIGHT,  110 },
};

static struct fh_operator *find_op(const char *name, unsigned int assoc_mask)
{
  for (int i = 0; i < ARRAY_SIZE(ops); i++) {
    if ((ops[i].assoc & assoc_mask) != 0 && strcmp(ops[i].name, name) == 0)
      return &ops[i];
  }
  return NULL;
}

struct fh_operator *fh_get_binary_op(const char *name)
{
  return find_op(name, FH_ASSOC_LEFT|FH_ASSOC_RIGHT);
}

struct fh_operator *fh_get_prefix_op(const char *name)
{
  return find_op(name, FH_ASSOC_PREFIX);
}

struct fh_operator *fh_get_op(const char *name)
{
  struct fh_operator *op;
  if ((op = fh_get_prefix_op(name)) != NULL)
    return op;
  if ((op = fh_get_binary_op(name)) != NULL)
    return op;
  return NULL;
}

struct fh_operator *fh_get_op_by_id(uint32_t op)
{
  for (int i = 0; i < ARRAY_SIZE(ops); i++)
    if (ops[i].op == op)
      return &ops[i];
  return NULL;
}

const char *fh_get_op_name(uint32_t op)
{
  struct fh_operator *opr = fh_get_op_by_id(op);
  if (opr == NULL)
    return NULL;
  return opr->name;
}

