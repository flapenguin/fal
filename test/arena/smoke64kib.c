#include "testlib.h"

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);
  arena_init(arena);

  assert(arena_SIZE == 65536u);
  assert(arena_BLOCK_SIZE == 16u);
  assert(arena_BEGIN == 64u);
  assert(arena_END == 4096u);
  assert(arena_TOTAL == 4032u);
  assert(arena_USER_LO_BYTES == 6u);
  assert(arena_USER_HI_BYTES == 8u);
  assert(arena_bumptop(arena) == arena_BEGIN);
}
