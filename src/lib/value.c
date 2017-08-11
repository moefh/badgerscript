/* value.c */

#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

static void free_func(struct fh_func *func)
{
  if (func->consts)
    free(func->consts);
  if (func->code)
    free(func->code);
  free(func);
}

static void free_array(struct fh_array *arr)
{
  if (arr->items)
    free(arr->items);
  free(arr);
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
    free_func(GET_OBJ_FUNC(obj));
    return;

  case FH_VAL_ARRAY:
    free_array(GET_OBJ_ARRAY(obj));
    return;
  }

  fprintf(stderr, "**** ERROR: freeing object of INVALID type %d\n", obj->obj.header.type);
  free(obj);
}


const char *fh_get_string(const struct fh_value *val)
{
  if (val->type != FH_VAL_STRING)
    return NULL;
  return GET_OBJ_STRING_DATA(val->data.obj);
}

int fh_get_array_len(const struct fh_value *val)
{
  if (val->type != FH_VAL_ARRAY)
    return -1;
  return GET_OBJ_ARRAY(val->data.obj)->len;
}

struct fh_value *fh_get_array_item(struct fh_value *val, int index)
{
  if (val->type != FH_VAL_ARRAY)
    return NULL;

  struct fh_array *arr = GET_OBJ_ARRAY(val->data.obj);
  if (index < 0 || index >= arr->len)
    return NULL;
  return &arr->items[index];
}

struct fh_value *fh_grow_array(struct fh_program *prog, struct fh_value *val, int num_items)
{
  if (val->type != FH_VAL_ARRAY)
    return NULL;

  struct fh_array *arr = GET_OBJ_ARRAY(val->data.obj);
  if (num_items <= 0
      || (size_t) arr->len + num_items + 15 < (size_t) arr->len
      || (size_t) arr->len + num_items + 15 > INT_MAX)
    return NULL;
  if (arr->len + num_items >= arr->cap) {
    size_t new_cap = ((size_t) arr->len + num_items + 15) / 16 * 16;
    void *new_items = realloc(arr->items, new_cap*sizeof(struct fh_value));
    if (! new_items) {
      fh_set_error(prog, "out of memory");
      return NULL;
    }
    arr->items = new_items;
    arr->cap = (int) new_cap;
  }
  struct fh_value *ret = &arr->items[arr->len];
  for (int i = 0; i < num_items; i++)
    ret[i].type = FH_VAL_NULL;
  arr->len += num_items;
  return ret;
}

/*************************************************************************
 * OBJECT CREATION       
 *
 * The following functions create a new object and add it to the list
 * of program objects.
 *************************************************************************/

static struct fh_object *fh_make_object(struct fh_program *prog, enum fh_value_type type, size_t size)
{
  if (size < sizeof(struct fh_object_header)) {
    fh_set_error(prog, "trying to create object with small size");
    return NULL;
  }
  
  struct fh_object *obj = malloc(size);
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
  struct fh_func *func = (struct fh_func *) fh_make_object(prog, FH_VAL_FUNC, sizeof(struct fh_func));
  if (! func)
    return NULL;
  func->gc_next_container = NULL;
  return func;
}

struct fh_array *fh_make_array(struct fh_program *prog)
{
  struct fh_array *arr = (struct fh_array *) fh_make_object(prog, FH_VAL_ARRAY, sizeof(struct fh_array));
  if (! arr)
    return NULL;
  arr->gc_next_container = NULL;
  arr->len = 0;
  arr->cap = 0;
  arr->items = NULL;
  return arr;
}

struct fh_object *fh_make_string(struct fh_program *prog, const char *str)
{
  return fh_make_string_n(prog, str, strlen(str)+1);
}

struct fh_object *fh_make_string_n(struct fh_program *prog, const char *str, size_t str_len)
{
  struct fh_object *obj = fh_make_object(prog, FH_VAL_STRING, sizeof(struct fh_string) + str_len);
  if (! obj)
    return NULL;
  memcpy(GET_OBJ_STRING_DATA(obj), str, str_len);
  GET_OBJ_STRING(obj)->size = str_len;
  return obj;
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
  struct fh_value *val = value_stack_push(&prog->c_vals, NULL);
  if (! val) {
    fh_set_error(prog, "out of memory");
    return prog->null_value;
  }
  struct fh_object *obj = fh_make_string_n(prog, str, str_len);
  if (! obj) {
    value_stack_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_STRING;
  val->data.obj = obj;
  return *val;
}

struct fh_value fh_new_array(struct fh_program *prog)
{
  struct fh_value *val = value_stack_push(&prog->c_vals, NULL);
  if (! val) {
    fh_set_error(prog, "out of memory");
    return prog->null_value;
  }
  struct fh_array *arr = fh_make_array(prog);
  if (! arr) {
    value_stack_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_ARRAY;
  val->data.obj = arr;
  return *val;
}
