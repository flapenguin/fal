#include "testlib.h"

typedef struct header_t {
  uint32_t a, b, c, d, e[3];
} header_t;

#define FAL_ARENA_DEF_BLOCK_POW   4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW         14u /* 16 KiB */
#define FAL_ARENA_DEF_HEADER_SIZE sizeof(header_t) /* 28 bytes = 2 blocks */
#define FAL_ARENA_DEF_NAME        arena
#include <fal/arena.h>

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);
  arena_init(arena);

  assert(arena_SIZE == 16384u);
  assert(arena_BLOCK_SIZE == 16u);
  assert(arena_BEGIN == 18u);
  assert(arena_END == 1024u);
  assert(arena_TOTAL == 1006u);
  assert(arena_USER_LO_BYTES == 0u);
  assert(arena_USER_HI_BYTES == 2u);
  assert(arena_bumptop(arena) == arena_BEGIN);
  assert(arena_header(arena) == (char*)arena + (arena_BEGIN-2) * arena_BLOCK_SIZE);
}
