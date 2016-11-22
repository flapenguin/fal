#include "common.h"

int main() {
  printf("Sample mark&sweep garbage collector based on fal/arena.h.\n");

  space_t* space = mkspace();
  space_init(space);

  object_t* root;

  /* Do some work */ {
    /* Allocate objects. */
    object_t* objs[16];
    alloc_objs(space, objs, FAL_ARRLEN(objs));

    /* Set root. */
    root = objs[1];
  }

  /* Mark phase */ {
    /* Somehow mark objects. */
    for (object_t* obj = root; obj; obj = obj->next) {
      space_mark(obj);
    }
  }

  printf("Before GC (but after mark):\n  root = %p\n", (void*)root);
  print_objs(space);

  /* Sweep phase */ {
    for (object_t* obj = space_first(space); obj; obj = space_next(obj)) {
      if (!space_marked(obj)) {
        space_free(obj);
      }
    }

    space_mark_all(space, 0);
  }

  printf("After GC:\n  root = %p\n", (void*)root);
  print_objs(space);
}
