/* gc.c */

#include <stdio.h>

#include "program.h"
#include "fh_i.h"

//#define DEBUG_GC

#ifdef DEBUG_GC
#define PRINT(x) x
#else
#define PRINT(x)
#endif

struct fh_gc_state {
  struct fh_object *container_list;
};

#ifdef DEBUG_GC
static void dump_obj(const char *prefix, struct fh_object *obj)
{
  printf("%s object of type %d", prefix, obj->obj.header.type);
  switch (obj->obj.header.type) {
  case FH_VAL_STRING:
    printf(" (string) ");
    fh_dump_string(GET_OBJ_STRING(obj));
    printf("\n");
    break;

  case FH_VAL_FUNC:
    printf(" (func)\n");
    break;

  default:
    printf(" (UNEXPECTED TYPE)\n");
  }
}
#endif

static void sweep(struct fh_program *prog)
{
  UNUSED(prog);
  PRINT(printf("***** sweeping\n");)
  struct fh_object **objs = &prog->objects;
  struct fh_object *cur;
  while ((cur = *objs) != NULL) {
    if (cur->obj.header.gc_mark) {
      cur->obj.header.gc_mark = 0;
      PRINT(dump_obj("-> keeping", cur);)
      objs = &cur->obj.header.next;
    } else {
      *objs = cur->obj.header.next;
      PRINT(dump_obj("-> freeing", cur);)
      fh_free_object(cur);
    }      
  }
}

void fh_free_program_objects(struct fh_program *prog)
{
  PRINT(printf("***** FREEING ALL OBJECTS\n");)
  struct fh_object *o = prog->objects;
  while (o) {
    struct fh_object *next = o->obj.header.next;
    PRINT(dump_obj("-> freeing", o);)
    fh_free_object(o);
    o = next;
  }
}

#define MARK_VALUE(gc, v) do { if (VAL_IS_OBJECT(v)) MARK_OBJECT((gc), (struct fh_object *)((v)->data.obj)); } while (0)
#define MARK_OBJECT(gc, o) do { if ((o)->obj.header.gc_mark == 0) mark_object((gc), (o)); } while (0)

static void mark_object(struct fh_gc_state *gc, struct fh_object *obj)
{
  PRINT(dump_obj("-> marking ", obj);)
  
  obj->obj.header.gc_mark = 1;
  switch (obj->obj.header.type) {
  case FH_VAL_STRING:
    break;
    
  case FH_VAL_FUNC:
    GET_OBJ_FUNC(obj)->gc_next_container = gc->container_list;
    gc->container_list = obj;
    break;

  default:
    fprintf(stderr, "GC ERROR: marking invalid object of type %d\n", obj->obj.header.type);
    break;
  }
}

static void mark_func_children(struct fh_gc_state *gc, struct fh_func *func)
{
  for (int i = 0; i < func->n_consts; i++)
    MARK_VALUE(gc, &func->consts[i]);
}

static void propagate_marks(struct fh_gc_state *gc)
{
  while (gc->container_list) {
    switch (gc->container_list->obj.header.type) {
    case FH_VAL_FUNC:
      {
        struct fh_func *f = GET_OBJ_FUNC(gc->container_list);
        gc->container_list = f->gc_next_container;
        mark_func_children(gc, f);
      }
      break;

    default:
      fprintf(stderr, "GC ERROR: propagating mark on invalid object of type %d\n", gc->container_list->obj.header.type);
      break;
    }
  }
}

static void mark(struct fh_program *prog)
{
  struct fh_gc_state gc;

  gc.container_list = NULL;
  
  // mark functions
  PRINT(printf("***** marking functions\n");)
  stack_foreach(struct fh_bc_func_info *, fi, &prog->bc.funcs) {
    MARK_OBJECT(&gc, (struct fh_object *) fi->func);
  }

  // mark stack
  struct fh_vm_call_frame *cur_frame = fh_stack_top(&prog->vm.call_stack);
  if (cur_frame) {
    int stack_size = cur_frame->base + ((cur_frame->func) ? cur_frame->func->n_regs : 0);
    struct fh_value *stack = prog->vm.stack;
    PRINT(printf("***** marking %d stack values\n", stack_size);)
    for (int i = 0; i < stack_size; i++)
      MARK_VALUE(&gc, &stack[i]);
  }

  // mark C values
  PRINT(printf("***** marking %d C tmp values\n", prog->c_vals.num);)
  stack_foreach(struct fh_value *, v, &prog->c_vals) {
    MARK_VALUE(&gc, v);
  }

  PRINT(printf("***** propagating marks\n");)
  propagate_marks(&gc);
}

void fh_collect_garbage(struct fh_program *prog)
{
  PRINT(printf("== STARTING GC ==================\n");)
  mark(prog);
  sweep(prog);
  PRINT(printf("== GC DONE ======================\n");)
}
