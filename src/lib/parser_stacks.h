/* parser_stacks.h */

#ifndef PARSER_STACKS_H_FILE
#define PARSER_STACKS_H_FILE

struct opr_info {
  struct opr_info *next;
  struct fh_operator *op;
  struct fh_src_loc loc;
};

static void opr_stack_free(struct opr_info *oprs)
{
  while (oprs != NULL) {
    struct opr_info *next = oprs->next;
    free(oprs);
    oprs = next;
  }
}

static int opr_stack_size(struct opr_info **oprs)
{
  int n = 0;
  for (struct opr_info *o = *oprs; o != NULL; o = o->next)
    n++;
  return n;
}

static int opr_stack_top(struct opr_info **oprs, struct fh_operator **op, struct fh_src_loc *loc)
{
  if (*oprs == NULL)
    return -1;
  struct opr_info *o = *oprs;
  if (op)
    *op = o->op;
  if (loc)
    *loc = o->loc;
  return 0;
}

static int pop_opr(struct opr_info **oprs)
{
  if (*oprs == NULL)
    return -1;
  struct opr_info *o = *oprs;
  *oprs = o->next;
  free(o);
  return 0;
}

static int push_opr(struct opr_info **oprs, struct fh_operator *op, struct fh_src_loc loc)
{
  struct opr_info *o = malloc(sizeof(struct opr_info));
  if (! o)
    return -1;
  o->op = op;
  o->loc = loc;
  o->next = *oprs;
  *oprs = o;
  return 0;
}

static int opn_stack_size(struct fh_p_expr **opns)
{
  int n = 0;
  for (struct fh_p_expr *e = *opns; e != NULL; e = e->next)
    n++;
  return n;
}

static struct fh_p_expr *pop_opn(struct fh_p_expr **opns)
{
  if (*opns == NULL)
    return NULL;
  struct fh_p_expr *e = *opns;
  *opns = e->next;
  e->next = NULL;
  return e;
}

static void push_opn(struct fh_p_expr **opns, struct fh_p_expr *expr)
{
  expr->next = *opns;
  *opns = expr;
}

#endif /* PARSER_STACKS_H_FILE */
