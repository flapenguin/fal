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

  fal_asserteq(arena_SIZE, 16384u, size_t, "%zu");
  fal_asserteq(arena_BLOCK_SIZE, 16u, size_t, "%zu");
  fal_asserteq(arena_BEGIN, 18u, size_t, "%zu");
  fal_asserteq(arena_END, 1024u, size_t, "%zu");
  fal_asserteq(arena_TOTAL, 1006u, size_t, "%zu");
  fal_asserteq(arena_USER_LO_BYTES, 0u, size_t, "%zu");
  fal_asserteq(arena_USER_HI_BYTES, 2u, size_t, "%zu");
  fal_asserteq(arena_bumptop(arena), arena_BEGIN, size_t, "%zu");
  fal_asserteq(arena_header(arena), (char*)arena + (arena_BEGIN-2) * arena_BLOCK_SIZE, void*, "%p");
  fal_asserteq(arena_mem_start(arena),
    (char*)arena + arena_BEGIN*arena_BLOCK_SIZE, void*, "%p");
  fal_asserteq(arena_mem_end(arena), (char*)arena + arena_SIZE, void*, "%p");
}
