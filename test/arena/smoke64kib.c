#include "testlib.h"

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);
  arena_init(arena);

  assert("Arena size must be 64 KiB" && arena_SIZE == 65536u);
  assert("Block size must be 16 bytes" && arena_BLOCK_SIZE == 16u);
  assert("First blok must be #64" && arena_FIRST == 64u);
  assert("Last block must be #4096" && arena_LAST == 4096u);
  assert("There're must be 4032 blocks in total" && arena_TOTAL == 4032u);
  assert("There're must be 6 lo-user bytes" && arena_USER_LO_BYTES == 6u);
  assert("There're must be 8 hi-user bytes" && arena_USER_HI_BYTES == 8u);
  assert("Bumptop must be first block."
    && arena_bumptop(arena) == arena_FIRST);
}
