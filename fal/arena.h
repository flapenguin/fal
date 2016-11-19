#ifndef __FAL_ARENA_H__
#define __FAL_ARENA_H__

/*
  Generic arena with configurable arena size, block size and typename.

  Compile-time parameters:
    (req) FAL_ARENA_DEF_NAME      - prefix for resulting type and functions
    (req) FAL_ARENA_DEF_ARENA_POW - power of arena size
                                    (i.e. 16 means 64 KiB arena)
    (req) FAL_ARENA_DEF_BLOCK_POW - power of block size
                                    (i.e. 4 means 16 byte blocks)

  Compile-time constraints:
    1. UnusedBits must be enough for at least 2 bytes.
                            2 * ArenaSize
          UnusedBits = ----------------------
                       CHAR_BIT * BlockSize^2

  Run-time constraints:
    1. Arena must be aligned to it size.

  Arena layout example is for 16 KiB arena with 16 byte blocks.
    XXXXYYYY AAAA~~~~AAAA ZZZZZZZZ BBBB~~~~BBBB OOOO~~~~OOOO

    16 KiB arena is split into 4096 blocks of 16 bytes each.
    First 64 blocks = 1024 bytes reserved for two bitmasks A and B
    (32 blocks = 512 bytes each) described below.
    At start of each bitmask 8 bytes remain unused and used for X, Y and Z data.
    Remaining 4032 blocks O are available for allocation.

    Bitsets A and B contain single bit per block.
    Bitset A means "flagged" and bitset B means "start of allocation".

    A | B
    0   0 Block is not allocated.
    0   1 Block is allocated and flag is unset.
    1   0 Block is a continuation of allocation.
    1   1 Block is allocated and flag is set.

    X space is used to store ix of first free block.
    Y and Z space is used for user data.
*/

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "utils.h"
#include "bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FAL_ARENA_DEF_BLOCK_POW) || !defined(FAL_ARENA_DEF_POW) || !defined(FAL_ARENA_DEF_NAME)
#error FAL_ARENA_DEF_BLOCK_POW, FAL_ARENA_DEF_POW and FAL_ARENA_DEF_NAME must be defined.
#endif

#define FAL_ARENA_T FAL_CONCAT(FAL_ARENA_DEF_NAME, _t)

#define FAL_ARENA_BLOCK_POW    FAL_CONCAT(FAL_ARENA_DEF_NAME, _BLOCK_POW)
#define FAL_ARENA_POW          FAL_CONCAT(FAL_ARENA_DEF_NAME, _POW)
#define FAL_ARENA_SIZE         FAL_CONCAT(FAL_ARENA_DEF_NAME, _SIZE)
#define FAL_ARENA_BLOCK_SIZE   FAL_CONCAT(FAL_ARENA_DEF_NAME, _BLOCK_SIZE)
#define FAL_ARENA_BLOCKS       FAL_CONCAT(FAL_ARENA_DEF_NAME, _BLOCKS)
#define FAL_ARENA_BITSET_SIZE  FAL_CONCAT(FAL_ARENA_DEF_NAME, _BITSET_SIZE)
#define FAL_ARENA_BLOCK_MASK   FAL_CONCAT(FAL_ARENA_DEF_NAME, _BLOCK_MASK)
#define FAL_ARENA_MASK         FAL_CONCAT(FAL_ARENA_DEF_NAME, _MASK)
#define FAL_ARENA_FIRST        FAL_CONCAT(FAL_ARENA_DEF_NAME, _FIRST)
#define FAL_ARENA_TOTAL        FAL_CONCAT(FAL_ARENA_DEF_NAME, _TOTAL)
#define FAL_ARENA_UNUSED_BITS  FAL_CONCAT(FAL_ARENA_DEF_NAME, _UNUSED_BITS)
#define FAL_ARENA_UNUSED_BYTES FAL_CONCAT(FAL_ARENA_DEF_NAME, _UNUSED_BYTES)

typedef struct FAL_ARENA_T FAL_ARENA_T;

enum FAL_CONCAT(FAL_ARENA_T, _defs) {
  FAL_ARENA_BLOCK_POW = FAL_ARENA_DEF_BLOCK_POW,
  FAL_ARENA_POW = FAL_ARENA_DEF_POW,
  FAL_ARENA_SIZE = 1u << FAL_ARENA_POW,
  FAL_ARENA_BLOCK_SIZE = 1u << FAL_ARENA_BLOCK_POW,
  FAL_ARENA_BLOCKS = FAL_ARENA_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA_BITSET_SIZE = FAL_ARENA_BLOCKS / CHAR_BIT,
  FAL_ARENA_BLOCK_MASK = FAL_ARENA_SIZE - 1,
  FAL_ARENA_MASK = ~FAL_ARENA_BLOCK_MASK,
  FAL_ARENA_FIRST = 2 * FAL_ARENA_BITSET_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA_TOTAL = FAL_ARENA_BLOCKS - FAL_ARENA_FIRST,
  FAL_ARENA_UNUSED_BITS = FAL_ARENA_FIRST,
  FAL_ARENA_UNUSED_BYTES = FAL_ARENA_BITSET_SIZE / CHAR_BIT
};

static inline void FAL_CONCAT(FAL_ARENA_DEF_NAME, __asertions)() {
  // Ensure there's enough unused bits at the beginning of each btiset to store additional data.
  FAL_STATIC_ASSERT(FAL_ARENA_UNUSED_BITS >= CHAR_BIT * 2);
}

static inline int FAL_CONCAT(FAL_ARENA_DEF_NAME, __ix_for)(void* ptr) {
  uintptr_t block = ((uintptr_t)ptr) & FAL_ARENA_BLOCK_MASK;
  return block >> FAL_ARENA_BLOCK_POW;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, __block)(FAL_ARENA_T* arena, int ix) {
  return (char*)arena + ix*FAL_ARENA_BLOCK_SIZE;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_a)(FAL_ARENA_T* arena) {
  return arena;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_b)(FAL_ARENA_T* arena) {
  return (char*)(void*)arena + FAL_ARENA_BITSET_SIZE;
}

static inline uint16_t* FAL_CONCAT(FAL_ARENA_DEF_NAME, __top_ptr)(FAL_ARENA_T* arena) {
  return (uint16_t*)(void*)arena;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, __markalloc)(FAL_ARENA_T* arena, size_t start, size_t size) {
  void* bitset_a = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_a)(arena);
  void* bitset_b = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_b)(arena);

  fal_bitset_clear(bitset_a, start);
  fal_bitset_set(bitset_b, start);

  for (size_t ix = 1; ix < size; ix++) {
    fal_bitset_set(bitset_a, start + ix);
    fal_bitset_clear(bitset_b, start + ix);
  }

  return FAL_CONCAT(FAL_ARENA_DEF_NAME, __block)(arena, start);
}

static inline FAL_ARENA_T* FAL_CONCAT(FAL_ARENA_DEF_NAME, _for)(void* ptr) {
  return (FAL_ARENA_T*)( ((uintptr_t)ptr) & FAL_ARENA_MASK );
}

static inline void FAL_CONCAT(FAL_ARENA_DEF_NAME, _init)(FAL_ARENA_T* arena) {
  assert(((uintptr_t)arena & FAL_ARENA_BLOCK_MASK) == 0
    && "[" FAL_STR(FAL_CONCAT(FAL_ARENA_DEF_NAME, _init)) "] arena is not aligned to its size");
  *FAL_CONCAT(FAL_ARENA_DEF_NAME, __top_ptr)(arena) = FAL_ARENA_FIRST;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, _bumpalloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL_CONCAT(FAL_ARENA_DEF_NAME, _bumpalloc)) "] size cannot be zero");
  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;
  uint16_t* top = FAL_CONCAT(FAL_ARENA_DEF_NAME, __top_ptr)(arena);
  if (*top + size >= FAL_ARENA_BLOCKS) {
    return 0;
  }

  void* result = FAL_CONCAT(FAL_ARENA_DEF_NAME, __markalloc)(arena, *top, size);
  *top += size;

  return result;
}

static inline void* FAL_CONCAT(FAL_ARENA_DEF_NAME, _alloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL_CONCAT(FAL_ARENA_DEF_NAME, _alloc)) "] size cannot be zero");
  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  void* bitset_a = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_a)(arena);
  void* bitset_b = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_b)(arena);

  size_t start = FAL_ARENA_FIRST;
  for (; start < FAL_ARENA_TOTAL; ) {
    size_t free = 0;
    for (; free < size; free++) {
      if (fal_bitset_test(bitset_a, start + free) || fal_bitset_test(bitset_b, start + free)) {
        break;
      }
    }

    if (free == size) {
      break;
    }

    start += free + 1;
  }

  if (start > FAL_ARENA_TOTAL) {
    return 0;
  }

  // Adjust dump allocation pointer.
  uint16_t* top = FAL_CONCAT(FAL_ARENA_DEF_NAME, __top_ptr)(arena);
  if (start >= *top) {
    *top += size;
  }

  return FAL_CONCAT(FAL_ARENA_DEF_NAME, __markalloc)(arena, start, size);
}

static inline void FAL_CONCAT(FAL_ARENA_DEF_NAME, _free)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL_CONCAT(FAL_ARENA_DEF_NAME, _free)) "] size cannot be zero");

  FAL_ARENA_T* arena = FAL_CONCAT(FAL_ARENA_DEF_NAME, _for)(ptr);
  size_t start = FAL_CONCAT(FAL_ARENA_DEF_NAME, __ix_for)(ptr);

  void* bitset_a = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_a)(arena);
  void* bitset_b = FAL_CONCAT(FAL_ARENA_DEF_NAME, __bitset_b)(arena);

  fal_bitset_clear(bitset_a, start);
  fal_bitset_clear(bitset_b, start);

  uint16_t* top = FAL_CONCAT(FAL_ARENA_DEF_NAME, __top_ptr)(arena);
  size_t end = start + 1;
  for (; !fal_bitset_test(bitset_b, end) && end < *top; end++) {
    fal_bitset_clear(bitset_a, end);
    fal_bitset_clear(bitset_b, end);
  }

  // Adjust dump allocation pointer.
  if (end >= *top) {
    *top = start;
  }
}

#undef FAL_ARENA_BLOCK_POW
#undef FAL_ARENA_POW
#undef FAL_ARENA_SIZE
#undef FAL_ARENA_BLOCK_SIZE
#undef FAL_ARENA_BLOCKS
#undef FAL_ARENA_BITSET_SIZE
#undef FAL_ARENA_BLOCK_MASK
#undef FAL_ARENA_MASK
#undef FAL_ARENA_FIRST
#undef FAL_ARENA_TOTAL
#undef FAL_ARENA_UNUSED_BITS
#undef FAL_ARENA_UNUSED_BYTES

#undef FAL_ARENA_DEF_BLOCK_POW
#undef FAL_ARENA_DEF_POW
#undef FAL_ARENA_DEF_NAME

#ifdef __cplusplus
}
#endif

#endif