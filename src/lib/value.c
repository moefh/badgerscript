/* value.c */

#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

static void free_bc_func(struct fh_func *func)
{
  if (func->consts)
    free(func->consts);
  if (func->code)
    free(func->code);
  free(func);
}

void fh_free_object(struct fh_object *obj)
{
  switch (obj->obj.header.type) {
  case FH_VAL_NULL:
  case FH_VAL_NUMBER:
  case FH_VAL_C_FUNC:
    fprintf(stderr, "**** ERROR: freeing object of NON-OBJECT type %d\n", obj->obj.header.type);
    free(obj);
    return;

  case FH_VAL_STRING:
    free(obj);
    return;
    
  case FH_VAL_FUNC:
    free_bc_func(&obj->obj.func);
    return;
  }

  fprintf(stderr, "**** ERROR: freeing object of INVALID type %d\n", obj->obj.header.type);
  free(obj);
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

/*************************************************************************
 * OBJECT CREATION       
 *
 * The following functions create a new object and add it to the list
 * of program objects.
 *************************************************************************/

static struct fh_object *fh_make_object(struct fh_program *prog, enum fh_value_type type, size_t extra_size)
{
  struct fh_object *obj = malloc(sizeof(struct fh_object) + extra_size);
  if (! obj) {
    fh_set_error(prog, "out of memory");
    return NULL;
  }
  obj->obj.header.next = prog->objects;
  prog->objects = obj;
  obj->obj.header.type = type;
  obj->obj.header.gc_mark = 0;
  return obj;
}

struct fh_func *fh_make_func(struct fh_program *prog)
{
  struct fh_func *func = (struct fh_func *) fh_make_object(prog, FH_VAL_FUNC, 0);
  if (! func)
    return NULL;
  func->gc_next_container = NULL;
  return func;
}

struct fh_string *fh_make_string(struct fh_program *prog, const char *str)
{
  return fh_make_string_n(prog, str, strlen(str)+1);
}

struct fh_string *fh_make_string_n(struct fh_program *prog, const char *str, size_t str_len)
{
  struct fh_object *obj = fh_make_object(prog, FH_VAL_STRING, str_len);
  if (! obj)
    return NULL;
  memcpy(GET_OBJ_STRING(obj), str, str_len);
  obj->obj.str.size = str_len;
  return (struct fh_string *) obj;
}

/*************************************************************************
 * C INTERFACE FUNCTIONS
 *
 * The following functions create a new value and, if the value is an
 * object, add the object to the C temp array to keep it anchored
 * while the C function is running.
 *************************************************************************/

struct fh_value fh_new_number(struct fh_program *prog, double num)
{
  UNUSED(prog);
  struct fh_value val;
  val.type = FH_VAL_NUMBER;
  val.data.num = num;
  return val;
}

struct fh_value fh_new_c_func(struct fh_program *prog, fh_c_func func)
{
  UNUSED(prog);
  struct fh_value val;
  val.type = FH_VAL_C_FUNC;
  val.data.c_func = func;
  return val;
}

struct fh_value fh_new_string(struct fh_program *prog, const char *str)
{
  return fh_new_string_n(prog, str, strlen(str) + 1);
}

struct fh_value fh_new_string_n(struct fh_program *prog, const char *str, size_t str_len)
{
  struct fh_value *val = fh_push(&prog->c_vals, NULL);
  if (! val) {
    fh_set_error(prog, "out of memory");
    return prog->null_value;
  }
  struct fh_string *s = fh_make_string_n(prog, str, str_len);
  if (! s) {
    fh_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_STRING;
  val->data.obj = s;
  return *val;
}
