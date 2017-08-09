/* value.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

static void free_bc_func(struct fh_func *func)
{
  if (func->consts) {
    free(func->consts);
  }
  if (func->code) {
    free(func->code);
  }
  free(func);
}

void fh_free_object(struct fh_object *obj)
{
  switch (obj->obj.header.type) {
  case FH_VAL_C_FUNC:
  case FH_VAL_NUMBER:
    fprintf(stderr, "**** WARNING: freeing object of VALUE type %d\n", obj->obj.header.type);
    free(obj);
    return;

  case FH_VAL_STRING: free(obj); return;
  case FH_VAL_FUNC: free_bc_func(&obj->obj.func); return;
  }

  fprintf(stderr, "**** WARNING: freeing object of INVALID type %d\n", obj->obj.header.type);
  free(obj);
}


struct fh_object *fh_new_object(struct fh_program *prog, enum fh_value_type type, size_t extra_size)
{
  struct fh_object *obj = malloc(sizeof(struct fh_object) + extra_size);
  if (! obj)
    return NULL;
  obj->obj.header.next = prog->objects;
  obj->obj.header.type = type;
  prog->objects = obj;
  return obj;
}

void fh_make_number(struct fh_program *prog, struct fh_value *val, double num)
{
  UNUSED(prog);
  val->type = FH_VAL_NUMBER;
  val->data.num = num;
}

int fh_make_string(struct fh_program *prog, struct fh_value *val, const char *str)
{
  size_t str_len = strlen(str);
  return fh_make_string_n(prog, val, str, str_len);
}

int fh_make_string_n(struct fh_program *prog, struct fh_value *val, const char *str, size_t str_len)
{
  struct fh_object *obj = fh_new_object(prog, FH_VAL_STRING, str_len + 1);
  if (! obj) {
    fh_set_error(prog, "out of memory");
    return -1;
  }
  memcpy(GET_OBJ_STRING(obj), str, str_len);
  GET_OBJ_STRING(obj)[str_len] = '\0';
  obj->obj.str.size = str_len;

  struct fh_value *v = fh_push(&prog->c_vals, NULL);
  if (! v) {
    free(obj);
    fh_set_error(prog, "out of memory");
    return -1;
  }
  v->type = FH_VAL_STRING;
  v->data.obj = obj;
  
  *val = *v;
  return 0;
}

void fh_make_c_func(struct fh_program *prog, struct fh_value *val, fh_c_func func)
{
  UNUSED(prog);
  val->type = FH_VAL_C_FUNC;
  val->data.c_func = func;
}

const char *fh_get_string(const struct fh_value *val)
{
  if (val->type != FH_VAL_STRING)
    return NULL;
  return GET_OBJ_STRING(val->data.obj);
}

struct fh_func *fh_get_func(const struct fh_value *val)
{
  if (val->type != FH_VAL_STRING)
    return NULL;
  return GET_OBJ_FUNC(val->data.obj);
}
