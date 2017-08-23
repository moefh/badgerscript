/* gc.c */

#include <stdio.h>

#include "fh_internal.h"
#include "program.h"

//#define COUNT_MEM_USAGE
//#define DEBUG_GC

#ifdef DEBUG_GC
#define debug_log(x)    printf(x)
#define debug_log1(x,y) printf(x,y)
#else
#define debug_log(x)
#define debug_log1(x,y)
#define debug_obj(x, y)
#endif

struct fh_gc_state {
  union fh_object *container_list;
#ifdef COUNT_MEM_USAGE
  size_t used;
  size_t released;
#endif
};

#ifdef DEBUG_GC
/* NOTE: we can't access any objects referenced by the object being
 * dumped, as they might already be free */
static void debug_obj(const char *prefix, union fh_object *obj)
{
  printf("%s object %p of type %d", prefix, (void *) obj, obj->header.type);
  switch (obj->header.type) {
  case FH_VAL_STRING:    printf(" (string) "); fh_dump_string(GET_OBJ_STRING_DATA(obj)); printf("\n"); break;
  case FH_VAL_UPVAL:     printf(" (upval)\n"); break;
  case FH_VAL_CLOSURE:   printf(" (closure)\n"); break;
  case FH_VAL_FUNC_DEF:  printf(" (func def)\n"); break;
  case FH_VAL_ARRAY:     printf(" (array of len %d)\n", GET_OBJ_ARRAY(obj)->len); break;
  case FH_VAL_MAP:       printf(" (map of len %d, cap %d)\n", GET_OBJ_MAP(obj)->len, GET_OBJ_MAP(obj)->cap); break;
  default:
    printf(" (UNEXPECTED TYPE)\n");
  }
}
#endif

#ifdef COUNT_MEM_USAGE
static inline size_t obj_size(union fh_object *obj)
{
  switch (obj->header.type) {
  case FH_VAL_STRING:  return sizeof(struct fh_string) + GET_OBJ_STRING(obj)->size;
  case FH_VAL_CLOSURE: return sizeof(struct fh_closure) + sizeof(struct fh_upval *)*GET_OBJ_CLOSURE(obj)->n_upvals;
  case FH_VAL_UPVAL:   return sizeof(struct fh_upval);
  case FH_VAL_ARRAY:   return sizeof(struct fh_array) + sizeof(struct fh_value)*GET_OBJ_ARRAY(obj)->cap;
  case FH_VAL_MAP:     return sizeof(struct fh_map) + sizeof(struct fh_map_entry)*GET_OBJ_MAP(obj)->cap;
  case FH_VAL_FUNC_DEF:
    {
      struct fh_func_def *f = GET_OBJ_FUNC_DEF(obj);
      return (sizeof(struct fh_func_def)
              + f->code_src_loc_size
              + f->code_size * sizeof(uint32_t)
              + f->n_consts * sizeof(struct fh_value)
              + f->n_upvals * sizeof(struct fh_upval_def));
    }
  default:
    return 0;
  }
}
#define count_used(gc, obj)     do { (gc)->used     += obj_size((obj)); } while (0)
#define count_released(gc, obj) do { (gc)->released += obj_size((obj)); } while (0)
#else
#define count_used(gc, obj)     (void)0
#define count_released(gc, obj) (void)0
#endif

static void sweep(struct fh_gc_state *gc, struct fh_program *prog)
{
  UNUSED(gc);
  debug_log("***** sweeping\n");
  union fh_object **objs = &prog->objects;
  union fh_object *cur;
  while ((cur = *objs) != NULL) {
    if (cur->header.gc_bits & (GC_BIT_MARK|GC_BIT_PIN)) {
      objs = &cur->header.next;
      cur->header.gc_bits &= ~GC_BIT_MARK;
      count_used(gc, cur);
      debug_obj("-> keeping", cur);
    } else {
      *objs = cur->header.next;
      count_released(gc, cur);
      debug_obj("-> FREEING", cur);
      fh_free_object(cur);
    }      
  }
}

void fh_free_program_objects(struct fh_program *prog)
{
  debug_log("***** FREEING ALL OBJECTS\n");
  union fh_object *o = prog->objects;
  while (o) {
    union fh_object *next = o->header.next;
    debug_obj("-> FREEING", o);
    fh_free_object(o);
    o = next;
  }
}

#define MARK_VALUE(gc, v) do { if (VAL_IS_OBJECT(v)) MARK_OBJECT((gc), (union fh_object *)((v)->data.obj)); } while (0)
#define MARK_OBJECT(gc, o) do { if (((o)->header.gc_bits&GC_BIT_MARK) == 0) mark_object((gc), (union fh_object *)(&(o)->header)); } while (0)

static void mark_object(struct fh_gc_state *gc, union fh_object *obj)
{
  debug_obj("-> marking", obj);
  
  GC_SET_BIT(obj, GC_BIT_MARK);
  switch (obj->header.type) {
  case FH_VAL_STRING:
    return;
    
  case FH_VAL_CLOSURE:
    GET_OBJ_CLOSURE(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    return;

  case FH_VAL_UPVAL:
    GET_OBJ_UPVAL(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    return;
    
  case FH_VAL_FUNC_DEF:
    GET_OBJ_FUNC_DEF(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    return;

  case FH_VAL_ARRAY:
    GET_OBJ_ARRAY(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    return;

  case FH_VAL_MAP:
    GET_OBJ_MAP(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    return;

  case FH_VAL_NULL:
  case FH_VAL_BOOL:
  case FH_VAL_NUMBER:
  case FH_VAL_C_FUNC:
    fprintf(stderr, "GC ERROR: marking non-object type %d\n", obj->header.type);
    return;
  }
    
  fprintf(stderr, "GC ERROR: marking invalid object type %d\n", obj->header.type);
}

static void mark_func_def_children(struct fh_gc_state *gc, struct fh_func_def *func_def)
{
  if (func_def->name)
    MARK_OBJECT(gc, func_def->name);
  for (int i = 0; i < func_def->n_consts; i++)
    MARK_VALUE(gc, &func_def->consts[i]);
}

static void mark_closure_children(struct fh_gc_state *gc, struct fh_closure *closure)
{
  MARK_OBJECT(gc, closure->func_def);
  for (int i = 0; i < closure->n_upvals; i++)
    MARK_OBJECT(gc, closure->upvals[i]);
}

static void mark_upval_children(struct fh_gc_state *gc, struct fh_upval *upval)
{
  MARK_VALUE(gc, upval->val);
  if (upval->val != &upval->data.storage && upval->data.next != NULL)
    MARK_OBJECT(gc, upval->data.next);
}

static void mark_array_children(struct fh_gc_state *gc, struct fh_array *arr)
{
  for (int i = 0; i < arr->len; i++)
    MARK_VALUE(gc, &arr->items[i]);
}

static void mark_map_children(struct fh_gc_state *gc, struct fh_map *map)
{
  for (uint32_t i = 0; i < map->cap; i++) {
    if (map->entries[i].key.type != FH_VAL_NULL) {
      MARK_VALUE(gc, &map->entries[i].key);
      MARK_VALUE(gc, &map->entries[i].val);
    }
  }
}

static void mark_container_children(struct fh_gc_state *gc)
{
  while (gc->container_list) {
    switch (gc->container_list->header.type) {
    case FH_VAL_CLOSURE:
      {
        struct fh_closure *c = GET_OBJ_CLOSURE(gc->container_list);
        gc->container_list = c->gc_next_container;
        mark_closure_children(gc, c);
      }
      continue;

    case FH_VAL_UPVAL:
      {
        struct fh_upval *uv = GET_OBJ_UPVAL(gc->container_list);
        gc->container_list = uv->gc_next_container;
        mark_upval_children(gc, uv);
      }
      continue;
      
    case FH_VAL_FUNC_DEF:
      {
        struct fh_func_def *f = GET_OBJ_FUNC_DEF(gc->container_list);
        gc->container_list = f->gc_next_container;
        mark_func_def_children(gc, f);
      }
      continue;
      
    case FH_VAL_ARRAY:
      {
        struct fh_array *a = GET_OBJ_ARRAY(gc->container_list);
        gc->container_list = a->gc_next_container;
        mark_array_children(gc, a);
      }
      continue;

    case FH_VAL_MAP:
      {
        struct fh_map *m = GET_OBJ_MAP(gc->container_list);
        gc->container_list = m->gc_next_container;
        mark_map_children(gc, m);
      }
      continue;

    case FH_VAL_NULL:
    case FH_VAL_BOOL:
    case FH_VAL_NUMBER:
    case FH_VAL_C_FUNC:
    case FH_VAL_STRING:
      fprintf(stderr, "GC ERROR: found non-container object (type %d)\n", gc->container_list->header.type);
      continue;
    }
    
    fprintf(stderr, "GC ERROR: found on invalid object of type %d\n", gc->container_list->header.type);
  }
}

static void mark_roots(struct fh_gc_state *gc, struct fh_program *prog)
{
  // mark global functions
  debug_log("***** marking global functions\n");
  stack_foreach(struct fh_closure *, *, pc, &prog->global_funcs) {
    MARK_OBJECT(gc, *pc);
  }

  // mark stack
  struct fh_vm_call_frame *cur_frame = call_frame_stack_top(&prog->vm.call_stack);
  if (cur_frame) {
    int stack_size = cur_frame->base + ((cur_frame->closure) ? cur_frame->closure->func_def->n_regs : 0);
    struct fh_value *stack = prog->vm.stack;
    debug_log1("***** marking %d stack values\n", stack_size);
    for (int i = 0; i < stack_size; i++)
      MARK_VALUE(gc, &stack[i]);
  }

  // mark open upvals
  debug_log("***** marking first open upval\n");
  if (prog->vm.open_upvals)
    MARK_OBJECT(gc, prog->vm.open_upvals);
  
  // mark pinned values
  debug_log1("***** marking %d pinned values\n", p_object_stack_size(&prog->pinned_objs));
  stack_foreach(union fh_object *, *, o, &prog->pinned_objs) {
    MARK_OBJECT(gc, *o);
  }

  // mark C values
  debug_log1("***** marking %d C tmp values\n", value_stack_size(&prog->c_vals));
  stack_foreach(struct fh_value, *, v, &prog->c_vals) {
    MARK_VALUE(gc, v);
  }
}

static void mark(struct fh_gc_state *gc, struct fh_program *prog)
{
  mark_roots(gc, prog);
  
  debug_log("***** marking container children\n");
  mark_container_children(gc);
}

void fh_collect_garbage(struct fh_program *prog)
{
  struct fh_gc_state gc = {
    .container_list = NULL,
  };

  debug_log("== STARTING GC ==================\n");
  mark(&gc, prog);
  sweep(&gc, prog);
  prog->n_created_objs_since_last_gc = 0;
  debug_log("== GC DONE ======================\n");

#ifdef COUNT_MEM_USAGE
  printf("gc used:     %zu\n", gc.used);
  printf("gc released: %zu\n", gc.released);
#endif
}
