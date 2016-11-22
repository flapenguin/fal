#include "common.h"

int main() {
  printf("Sample semispace garbage collector based on fal/arena.h.\n");

  space_t* oldspace = mkspace();
  space_t* newspace = mkspace();

  object_t* root;
  /* Do some work */ {
    space_init(oldspace);

    /* Allocate some objects with references in the OLD space. */
    object_t* objs[16];
    alloc_objs(oldspace, objs, FAL_ARRLEN(objs));

    /* Set root. */
    root = objs[0];
  }

  /* Mark phase */ {
    /* Somehow mark objects in the OLD space. */
    for (object_t* obj = root; obj; obj = obj->next) {
      space_mark(obj);
    }
  }

  printf("Before GC (but after mark):\n  root = %p\n", (void*)root);
  print_objs(oldspace);

  /* Collect phase */ {
    space_init(newspace);

    /* Run copying garbage collector and move marked objects to NEW space. */
    for (object_t* obj = space_first(oldspace); obj; obj = space_next(obj)) {
      if (!space_marked(obj)) {
        continue;
      }

      /* Create copy of the object in NEW space. */
      size_t size = space_size(obj);
      void* newobj = space_bumpalloc(newspace, size);
      memcpy(newobj, obj, size);

      /* Object memory in OLD space will now point to location
        of the same object in NEW space. */
      *(object_t**)obj = newobj;
    }

    /* After all objects are moved to NEW space fix object references. */
    for (object_t* obj = space_first(newspace); obj; obj = space_next(obj)) {
      /* NULLs don't need to be fixed. */
      if (obj->next) {
        obj->next = *(object_t**)obj->next;
      }
    }

    /* Fix root. */
    root = *(object_t**)root;

    /* Space OLD and NEW spaces. */
    {
      space_t* tmp = oldspace;
      oldspace = newspace;
      newspace = tmp;
    }
  }

  printf("After GC:\n  root = %p\n", (void*)root);
  print_objs(oldspace);
}
