#include "testlib.h"
#include <string.h>

typedef struct header_t {
  char str[24];
} header_t;

#define FAL_ARENA_DEF_BLOCK_POW   4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW         12u /* 4 KiB */
#define FAL_ARENA_DEF_HEADER_SIZE sizeof(header_t) /* 24 bytes = 2 blocks */
#define FAL_ARENA_DEF_INCOMPACT   /* +2 bytes, still 2 blocks */
#define FAL_ARENA_DEF_NAME        arena
#include <fal/arena.h>

static const char* lorem = "Lorem ipsum dolor sit.";

int main() {
  arena_t* arena = (arena_t*)testlib_alloc_arena(arena_SIZE);
  arena_init(arena);

  fal_asserteq(sizeof(header_t), 24u, size_t, "%zu");
  strcpy(((header_t*)arena_header(arena))->str, lorem);

  void* x = arena_bumpalloc(arena, 1024);
  assert(x);

  assert(strcmp(((header_t*)arena_header(arena))->str, lorem) == 0);

  arena_init(arena);

  assert(strcmp(((header_t*)arena_header(arena))->str, lorem) == 0);
}
