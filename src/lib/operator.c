/* operator.h */

#include <stdlib.h>
#include <string.h>

#include "fh_internal.h"
#include "stack.h"

void fh_init_op_table(struct fh_op_table *ops)
{
  op_stack_init(&ops->prefix);
  op_stack_init(&ops->binary);
}

void fh_destroy_op_table(struct fh_op_table *ops)
{
  op_stack_free(&ops->prefix);
  op_stack_free(&ops->binary);
}

int fh_add_op(struct fh_op_table *ops, uint32_t op, char *name, int32_t prec, enum fh_op_assoc assoc)
{
  struct fh_operator opr;

  if (strlen(name) >= sizeof(opr.name))
    return -1;

  opr.op = op;
  opr.prec = prec;
  opr.assoc = assoc;
  strcpy(opr.name, name);

  if (assoc == FH_ASSOC_PREFIX)
    return op_stack_push(&ops->prefix, &opr) ? 0 : -1;
  if (assoc == FH_ASSOC_LEFT || assoc == FH_ASSOC_RIGHT)
    return op_stack_push(&ops->binary, &opr) ? 0 : -1;
  return -1;
}

static struct fh_operator *find_op(struct op_stack *s, char *name)
{
  stack_foreach(struct fh_operator, *, op, s) {
    if (strcmp(op->name, name) == 0)
      return op;
  }
  return NULL;
}

struct fh_operator *fh_get_binary_op(struct fh_op_table *ops, char *name)
{
  return find_op(&ops->binary, name);
}

struct fh_operator *fh_get_prefix_op(struct fh_op_table *ops, char *name)
{
  return find_op(&ops->prefix, name);
}

struct fh_operator *fh_get_op(struct fh_op_table *ops, char *name)
{
  struct fh_operator *op;
  if ((op = fh_get_prefix_op(ops, name)) != NULL)
    return op;
  if ((op = fh_get_binary_op(ops, name)) != NULL)
    return op;
  return NULL;
}

struct fh_operator *fh_get_op_by_id(struct fh_op_table *ops, uint32_t op)
{
  stack_foreach(struct fh_operator, *, opr, &ops->binary) {
    if (opr->op == op)
      return opr;
  }
  stack_foreach(struct fh_operator, *, opr, &ops->prefix) {
    if (opr->op == op)
      return opr;
  }
  return NULL;
}
