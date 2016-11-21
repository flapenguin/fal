#include "testlib.h"

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);
  arena_init(arena);

  assert("arena_first() must return NULL for empty arena"
    && !arena_first(arena));
  assert("arena_next(NULL) must return NULL" && !arena_next(0));
  assert("arena_next_noskip(NULL) must return NULL" && !arena_next_noskip(0));

  assert("arena_first_noskip() must return first block of empty arena"
    && arena_first_noskip(arena) == ((char*)arena + arena_FIRST*arena_BLOCK_SIZE));
  assert("arena_size(arena_first_noskip()) must return arena_TOTAL"
    && arena_size(arena_first_noskip(arena)) == arena_TOTAL);

  assert("arena_next(arena_first_noskip()) must return NULL"
    && !arena_next(arena_first_noskip(arena)));

  assert("arena_next_noskip(arena_first_noskip()) must return NULL"
    && !arena_next_noskip(arena_first_noskip(arena)));

  void* a = arena_alloc(arena, arena_BLOCK_SIZE * 4);
  void* b = arena_alloc(arena, arena_BLOCK_SIZE * 4);
  void* c = arena_alloc(arena, arena_BLOCK_SIZE * (arena_TOTAL - 8));

  assert(a && b && c);

  assert("arena_first() and ~_noskip() must return same value if first block is allocated"
    && arena_first(arena) == arena_first_noskip(arena)
    && arena_first(arena) == a);

  {
    void* first = arena_first(arena);
    void* second = arena_next(first);
    void* third = arena_next(second);
    void* fourth = arena_next(third);

    assert("iterating"
      && first == a && second == b && third == c && !fourth);
  }

  {
    void* first = arena_first(arena);
    void* second = arena_next_noskip(first);
    void* third = arena_next_noskip(second);
    void* fourth = arena_next_noskip(third);

    assert("iterating noskip"
      && first == a && second == b && third == c && !fourth);
  }

  arena_free(a);
  {
    void* first = arena_first(arena);
    void* second = arena_next(first);
    void* third = arena_next(second);

    assert("iterating after freeing"
      && first == b && second == c && !third);
  }

  {
    void* first = arena_first_noskip(arena);
    void* second = arena_next_noskip(first);
    void* third = arena_next_noskip(second);
    void* fourth = arena_next_noskip(third);

    assert("iterating noskip after freeing"
      && first == a && second == b && third == c && !fourth);
  }
}
