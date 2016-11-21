#include "testlib.h"

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);

  {
    arena_init(arena);
    assert(arena_bumptop(arena) == arena_FIRST);

    void* a = arena_bumpalloc(arena, arena_BLOCK_SIZE * 4);
    assert(a && arena_bumptop(arena) == arena_FIRST + 4);

    void* b = arena_bumpalloc(arena, arena_BLOCK_SIZE * 4);
    assert(b && arena_bumptop(arena) == arena_FIRST + 8);

    void* c = arena_bumpalloc(arena, arena_BLOCK_SIZE * (arena_TOTAL - 8));
    assert(c && arena_bumptop(arena) == arena_LAST);

    assert(!arena_bumpalloc(arena, arena_BLOCK_SIZE * 4));
    assert(arena_bumptop(arena) == arena_LAST);

    arena_free(a);
    assert(arena_first(arena) == b);

    void* a0 = arena_bumpalloc(arena, arena_BLOCK_SIZE * 4);
    assert(!a0);
    assert(arena_bumptop(arena) == arena_LAST);

    void* a1 = arena_alloc(arena, arena_BLOCK_SIZE * 2);
    void* a2 = arena_alloc(arena, arena_BLOCK_SIZE * 2);
    assert(arena_bumptop(arena) == arena_LAST);
    assert(a1 == a && a2 == (char*)a + arena_BLOCK_SIZE * 2);
  }

  {
    arena_init(arena);

    void* a = arena_alloc(arena, arena_BLOCK_SIZE * 4);
    void* b = arena_alloc(arena, arena_BLOCK_SIZE * 4);;
    assert(a && b);

    arena_free(b);
    assert(arena_bumptop(arena) == arena_FIRST + 4);

    arena_free(a);
    assert(arena_bumptop(arena) == arena_FIRST);
  }

  {
    arena_init(arena);

    void* a = arena_alloc(arena, arena_BLOCK_SIZE * 4);
    void* b = arena_alloc(arena, arena_BLOCK_SIZE * 4);;
    assert(a && b);

    arena_free(a);
    assert(arena_bumptop(arena) == arena_FIRST + 8);

    arena_free(b);
    assert(arena_bumptop(arena) == arena_FIRST);
  }
}
