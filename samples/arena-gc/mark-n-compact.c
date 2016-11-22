#define NEWPOS
#include "common.h"

int main() {
  printf("Sample mark&compact garbage collector based on fal/arena.h.\n");

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

  /* Calculate new positions for marked objects */ {
    char* newpos = space_first_noskip(space);
    for (object_t* obj = space_first(space); obj; obj = space_next(obj)) {
      if (!space_marked(obj)) {
        continue;
      }

      obj->newpos = newpos;
      newpos += space_size(obj);
    }
  }

  printf("Before GC (but after mark):\n  root = %p\n", (void*)root);
  print_objs(space);

  /* Update references */ {
    for (object_t* obj = space_first(space); obj; obj = space_next(obj)) {
      if (space_marked(obj) && obj->next) {
        printf(":: fix %p->next from %p to %p\n", (void*)obj, (void*)obj->next, (void*)obj->next->newpos);
        obj->next = obj->next->newpos;
      }
    }

    root = root->newpos;
  }

  /* Compact phase */ {
    char* newpos;
    size_t size;
    object_t* next;
    for (object_t* obj = space_first(space); obj; obj = next) {
      next = space_next(obj);
      if (!space_marked(obj)) {
        continue;
      }

      printf(":: moving %p to %p\n", (void*)obj, (void*)obj->newpos);
      if (obj == obj->newpos) {
        continue;
      }

      newpos = obj->newpos;
      size = space_size(obj);
      space_emplace(newpos, size);
      memmove(newpos, obj, size);
    }

    space_emplace_end(newpos + size);
    space_mark_all(space, 0);
  }

  printf("After GC:\n  root = %p\n", (void*)root);
  print_objs(space);
}
