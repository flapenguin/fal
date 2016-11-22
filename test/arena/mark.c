#include "testlib.h"

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);

  arena_init(arena);
  assert(arena_bumptop(arena) == arena_BEGIN);

  void* p[] = {
    arena_bumpalloc(arena, 64),
    arena_bumpalloc(arena, 64),
    arena_bumpalloc(arena, 64),
    arena_bumpalloc(arena, 64),
    arena_bumpalloc(arena, arena_BLOCK_SIZE*arena_TOTAL - 256),
  };

  void *a = p[0], *b = p[1], *c = p[2], *d = p[3], *e = p[4];

  assert(a && b && c && d && e);
  assert(arena_bumptop(arena) == arena_END);

  for (int i = 0; i < (int)FAL_ARRLEN(p); i++) {
    assert(!arena_marked(p[i]));
  }

  arena_mark(a);
  arena_mark(c);
  arena_mark(e);

  for (int i = 0; i < (int)FAL_ARRLEN(p); i++) {
    assert(!!arena_marked(p[i]) == (i % 2 == 0));
  }

  arena_mark_all(arena, 1);
  for (int i = 0; i < (int)FAL_ARRLEN(p); i++) {
    assert(arena_marked(p[i]));
  }

  arena_mark_all(arena, 0);
  for (int i = 0; i < (int)FAL_ARRLEN(p); i++) {
    assert(!arena_marked(p[i]));
  }
}
