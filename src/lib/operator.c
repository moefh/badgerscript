/* operator.h */

#include <stdlib.h>
#include <string.h>

#include "fh_i.h"
#include "stack.h"

void fh_init_op_table(struct fh_op_table *ops)
{
  fh_init_stack(&ops->prefix, sizeof(struct fh_operator));
  fh_init_stack(&ops->binary, sizeof(struct fh_operator));
}

void fh_free_op_table(struct fh_op_table *ops)
{
  fh_free_stack(&ops->prefix);
  fh_free_stack(&ops->binary);
}

int fh_add_op(struct fh_op_table *ops, char *name, int32_t prec, enum fh_op_assoc assoc)
{
  struct fh_operator op;

  if (strlen(name) >= 4)
    return -1;

  memset(op.name.str, 0, 4);
  strcpy(op.name.str, name);
  op.prec = prec;
  op.assoc = assoc;

  if (assoc == FH_ASSOC_PREFIX)
    return fh_push(&ops->prefix, &op);
  if (assoc == FH_ASSOC_LEFT || assoc == FH_ASSOC_RIGHT)
    return fh_push(&ops->binary, &op);
  return -1;
}

static struct fh_operator *find_op(struct fh_stack *s, char *name)
{
  struct fh_operator *ops = (struct fh_operator *) s->data;
  for (int i = 0; i < s->num; i++) {
    if (strcmp(ops[i].name.str, name) == 0)
      return &ops[i];
  }
  return NULL;
}

struct fh_operator *fh_get_binary_op(struct fh_op_table *ops, char *name)
{
  return find_op(&ops->binary, name);
}

struct fh_operator *fh_get_prefix_op(struct fh_op_table *ops, char *name)
{
  return find_op(&ops->binary, name);
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
