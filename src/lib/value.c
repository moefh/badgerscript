/* value.c */

#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "program.h"
#include "value.h"

static void free_func_def(struct fh_func_def *func_def)
{
  if (func_def->consts)
    free(func_def->consts);
  if (func_def->code)
    free(func_def->code);
  if (func_def->upvals)
    free(func_def->upvals);
  if (func_def->code_src_loc)
    free(func_def->code_src_loc);
  free(func_def);
}

static void free_closure(struct fh_closure *closure)
{
  free(closure);
}

static void free_upval(struct fh_upval *upval)
{
  free(upval);
}

static void free_array(struct fh_array *arr)
{
  if (arr->items)
    free(arr->items);
  free(arr);
}

static void free_map(struct fh_map *map)
{
  if (map->entries)
    free(map->entries);
  free(map);
}

void fh_free_object(struct fh_object *obj)
{
  switch (obj->obj.header.type) {
  case FH_VAL_NULL:
  case FH_VAL_BOOL:
  case FH_VAL_NUMBER:
  case FH_VAL_C_FUNC:
    fprintf(stderr, "**** ERROR: freeing object of NON-OBJECT type %d\n", obj->obj.header.type);
    free(obj);
    return;

  case FH_VAL_STRING:    free(obj); return;
  case FH_VAL_CLOSURE:   free_closure(GET_OBJ_CLOSURE(obj)); return;
  case FH_VAL_UPVAL:     free_upval(GET_OBJ_UPVAL(obj)); return;
  case FH_VAL_FUNC_DEF:  free_func_def(GET_OBJ_FUNC_DEF(obj)); return;
  case FH_VAL_ARRAY:     free_array(GET_OBJ_ARRAY(obj)); return;
  case FH_VAL_MAP:       free_map(GET_OBJ_MAP(obj)); return;
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

struct fh_value *fh_grow_array_object(struct fh_program *prog, struct fh_array *arr, int num_items)
{
  if (arr->type != FH_VAL_ARRAY)
    return NULL;

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

struct fh_value *fh_grow_array(struct fh_program *prog, struct fh_value *val, int num_items)
{
  return fh_grow_array_object(prog, GET_OBJ_ARRAY(val->data.obj), num_items);
}

const char *fh_get_func_def_name(struct fh_func_def *func_def)
{
  if (func_def->type != FH_VAL_FUNC_DEF || ! func_def->name)
    return NULL;
  return GET_OBJ_STRING_DATA(func_def->name);
}

/*************************************************************************
 * OBJECT CREATION       
 *
 * The following functions create a new object and add it to the list
 * of program objects.
 *************************************************************************/

static void *fh_make_object(struct fh_program *prog, bool pinned, enum fh_value_type type, size_t size)
{
  if (size < sizeof(struct fh_object_header)) {
    fh_set_error(prog, "object size too small");
    return NULL;
  }

  if (prog->gc_frequency >= 0 && ++prog->n_created_objs_since_last_gc > prog->gc_frequency)
    fh_collect_garbage(prog);
  
  struct fh_object *obj = malloc(size);
  if (! obj) {
    fh_set_error(prog, "out of memory");
    return NULL;
  }
  if (pinned) {
    if (! p_object_stack_push(&prog->pinned_objs, &obj)) {
      free(obj);
      fh_set_error(prog, "out of memory");
      return NULL;
    }
  }

  obj->obj.header.next = prog->objects;
  prog->objects = obj;
  obj->obj.header.type = type;
  obj->obj.header.gc_bits = 0;
  return obj;
}

struct fh_upval *fh_make_upval(struct fh_program *prog, bool pinned)
{
  struct fh_upval *uv = fh_make_object(prog, pinned, FH_VAL_UPVAL, sizeof(struct fh_upval));
  if (! uv)
    return NULL;
  uv->gc_next_container = NULL;
  return uv;
}

struct fh_closure *fh_make_closure(struct fh_program *prog, bool pinned, struct fh_func_def *func_def)
{
  struct fh_closure *c = fh_make_object(prog, pinned, FH_VAL_CLOSURE, sizeof(struct fh_closure) + func_def->n_upvals*sizeof(struct fh_upval *));
  if (! c)
    return NULL;
  c->gc_next_container = NULL;
  c->func_def = func_def;
  c->n_upvals = func_def->n_upvals;
  if (c->n_upvals > 0)
    c->upvals = (struct fh_upval **) ((char*)c + sizeof(struct fh_closure));
  else
    c->upvals = NULL;
  return c;
}

struct fh_func_def *fh_make_func_def(struct fh_program *prog, bool pinned)
{
  struct fh_func_def *func_def = fh_make_object(prog, pinned, FH_VAL_FUNC_DEF, sizeof(struct fh_func_def));
  if (! func_def)
    return NULL;
  func_def->gc_next_container = NULL;
  return func_def;
}

struct fh_array *fh_make_array(struct fh_program *prog, bool pinned)
{
  struct fh_array *arr = fh_make_object(prog, pinned, FH_VAL_ARRAY, sizeof(struct fh_array));
  if (! arr)
    return NULL;
  arr->gc_next_container = NULL;
  arr->len = 0;
  arr->cap = 0;
  arr->items = NULL;
  return arr;
}

struct fh_map *fh_make_map(struct fh_program *prog, bool pinned)
{
  struct fh_map *map = fh_make_object(prog, pinned, FH_VAL_MAP, sizeof(struct fh_map));
  if (! map)
    return NULL;
  map->gc_next_container = NULL;
  map->len = 0;
  map->cap = 0;
  map->entries = NULL;
  return map;
}

struct fh_string *fh_make_string_n(struct fh_program *prog, bool pinned, const char *str, size_t str_len)
{
  if (sizeof(struct fh_string) + str_len > UINT32_MAX)
    return NULL;
  struct fh_string *s = fh_make_object(prog, pinned, FH_VAL_STRING, sizeof(struct fh_string) + str_len);
  if (! s)
    return NULL;
  memcpy(GET_OBJ_STRING_DATA(s), str, str_len);
  s->size = (uint32_t) str_len;
  s->hash = fh_hash(str, str_len);
  return s;
}

struct fh_string *fh_make_string(struct fh_program *prog, bool pinned, const char *str)
{
  return fh_make_string_n(prog, pinned, str, strlen(str)+1);
}

/*************************************************************************
 * C INTERFACE FUNCTIONS
 *
 * The following functions create a new value and, if the value is an
 * object, add the object to the C temp array to keep it anchored
 * while the C function is running.
 *************************************************************************/

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
  struct fh_string *s = fh_make_string_n(prog, false, str, str_len);
  if (! s) {
    value_stack_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_STRING;
  val->data.obj = s;
  return *val;
}

struct fh_value fh_new_array(struct fh_program *prog)
{
  struct fh_value *val = value_stack_push(&prog->c_vals, NULL);
  if (! val) {
    fh_set_error(prog, "out of memory");
    return prog->null_value;
  }
  struct fh_array *arr = fh_make_array(prog, false);
  if (! arr) {
    value_stack_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_ARRAY;
  val->data.obj = arr;
  return *val;
}

struct fh_value fh_new_map(struct fh_program *prog)
{
  struct fh_value *val = value_stack_push(&prog->c_vals, NULL);
  if (! val) {
    fh_set_error(prog, "out of memory");
    return prog->null_value;
  }
  struct fh_map *map = fh_make_map(prog, false);
  if (! map) {
    value_stack_pop(&prog->c_vals, NULL);
    return prog->null_value;
  }
  val->type = FH_VAL_MAP;
  val->data.obj = map;
  return *val;
}
