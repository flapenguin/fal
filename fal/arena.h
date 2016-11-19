#ifndef __FAL_ARENA_H__
#define __FAL_ARENA_H__

/*
  Generic arena with configurable arena size, block size and typename.

  API:
    arena_ prefix is overriden by <FAL_ARENA_DEF_NAME>_.
    Everything with __ (two underscores) in name should be considered internal.

    Types:
      arena_t - opaque struct, should be used only as arena_t*.

    Initializing:
      void arena_init(arena_t*)
        (re)initialize arena in memory

    Allocating:
      void* arena_bumpalloc(arena_t*, size_t)
        allocate memory at the end of the arena or return 0 if arena is full
      void* arena_alloc(arena_t*, size_t)
        tries arena_bumpalloc and fallbacks to looking for freed blocks
      void arena_free(void*)
        frees passed allocation

    Marking:
      void arena_mark(void*)
        mark allocarion
      void arena_unmark(void*)
        unmark allocation
      int arena_marked(void*)
        check if allocation is marked

    Querying:
      int arena_used(void*)
        check if memory is allocated
      size_t arena_size(void*)
        get size (in blocks) of allocation
      arena_t* arena_for(void*)
        get arena used to allocate passed memory

    Iterating:
      void* arena_first(arena_t*)
        get first allocation
      void* arena_next_noskip(void*)
        get next allocation, including freed blocks (use arena_used)
      void* arena_next(void*)
        get next allocation, skipping freed blocks

    Constants:
      arena_SIZE       - arena size in bytes
      arena_BLOCK_SIZE - block size in bytes
      arena_TOTAL      - nubmer of blocks available for allocation

  Compile-time parameters:
    (req) FAL_ARENA_DEF_NAME      - prefix for resulting type and functions
    (req) FAL_ARENA_DEF_ARENA_POW - power of arena size
                                    (i.e. 16 means 64 KiB arena)
    (req) FAL_ARENA_DEF_BLOCK_POW - power of block size
                                    (i.e. 4 means 16 byte blocks)

    (opt) FAL_ARENA_DEF_NO_UNDEF  - do not undefined all compile-time parameters

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
    0   0 Free block.
    0   1 Start of allocation, flag is unset.
    1   0 Allocation extension.
    1   1 Start of allocation, flag is set.

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

#define FAL__T(X) FAL_CONCAT(FAL_ARENA_DEF_NAME, X)
#define FAL_ARENA_T FAL__T(_t)

/* Public */
#define FAL_ARENA_TOTAL         FAL__T(_TOTAL)
#define FAL_ARENA_SIZE          FAL__T(_SIZE)
#define FAL_ARENA_BLOCK_SIZE    FAL__T(_BLOCK_SIZE)
/* Internal */
#define FAL_ARENA__BLOCK_POW    FAL__T(__BLOCK_POW)
#define FAL_ARENA__POW          FAL__T(__POW)
#define FAL_ARENA__BLOCKS       FAL__T(__BLOCKS)
#define FAL_ARENA__BITSET_SIZE  FAL__T(__BITSET_SIZE)
#define FAL_ARENA__BLOCK_MASK   FAL__T(__BLOCK_MASK)
#define FAL_ARENA__MASK         FAL__T(__MASK)
#define FAL_ARENA__FIRST        FAL__T(__FIRST)
#define FAL_ARENA__UNUSED_BITS  FAL__T(__UNUSED_BITS)
#define FAL_ARENA__UNUSED_BYTES FAL__T(__UNUSED_BYTES)

typedef struct FAL_ARENA_T FAL_ARENA_T;

enum FAL_CONCAT(FAL_ARENA_T, _defs) {
  FAL_ARENA__BLOCK_POW = FAL_ARENA_DEF_BLOCK_POW,
  FAL_ARENA__POW = FAL_ARENA_DEF_POW,
  FAL_ARENA_SIZE = 1u << FAL_ARENA__POW,
  FAL_ARENA_BLOCK_SIZE = 1u << FAL_ARENA__BLOCK_POW,
  FAL_ARENA__BLOCKS = FAL_ARENA_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA__BITSET_SIZE = FAL_ARENA__BLOCKS / CHAR_BIT,
  FAL_ARENA__BLOCK_MASK = FAL_ARENA_SIZE - 1,
  FAL_ARENA__MASK = ~FAL_ARENA__BLOCK_MASK,
  FAL_ARENA__FIRST = 2 * FAL_ARENA__BITSET_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA_TOTAL = FAL_ARENA__BLOCKS - FAL_ARENA__FIRST,
  FAL_ARENA__UNUSED_BITS = FAL_ARENA__FIRST,
  FAL_ARENA__UNUSED_BYTES = FAL_ARENA__UNUSED_BITS / CHAR_BIT
};

static inline void FAL__T(__asertions)() {
  /* Ensure there's enough unused bits at the beginning of each btiset to store additional data. */
  FAL_STATIC_ASSERT(FAL_ARENA__UNUSED_BITS >= CHAR_BIT * 2);
}

static inline int FAL__T(__ix_for)(void* ptr) {
  uintptr_t block = ((uintptr_t)ptr) & FAL_ARENA__BLOCK_MASK;
  return block >> FAL_ARENA__BLOCK_POW;
}

static inline void* FAL__T(__block)(FAL_ARENA_T* arena, int ix) {
  return (char*)arena + ix*FAL_ARENA_BLOCK_SIZE;
}

static inline void* FAL__T(__bitset_a)(FAL_ARENA_T* arena) {
  return arena;
}

static inline void* FAL__T(__bitset_b)(FAL_ARENA_T* arena) {
  return (char*)(void*)arena + FAL_ARENA__BITSET_SIZE;
}

static inline uint16_t* FAL__T(__top_ptr)(FAL_ARENA_T* arena) {
  return (uint16_t*)(void*)arena;
}

static inline void* FAL__T(__markalloc)(FAL_ARENA_T* arena, size_t start, size_t size) {
  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  fal_bitset_clear(bitset_a, start);
  fal_bitset_set(bitset_b, start);

  for (size_t ix = 1; ix < size; ix++) {
    fal_bitset_set(bitset_a, start + ix);
    fal_bitset_clear(bitset_b, start + ix);
  }

  return FAL__T(__block)(arena, start);
}

static inline int FAL__T(__is_guts)(void* bitset_a, void* bitset_b, size_t ix) {
  return fal_bitset_test(bitset_a, ix) && !fal_bitset_test(bitset_b, ix);
}

static inline int FAL__T(__is_start)(void* bitset_a, void* bitset_b, size_t ix) {
  return fal_bitset_test(bitset_b, ix);
}

static inline int FAL__T(__is_free)(void* bitset_a, void* bitset_b, size_t ix) {
  return !fal_bitset_test(bitset_a, ix) && !fal_bitset_test(bitset_b, ix);
}

static inline size_t FAL__T(__size)(void* bitset_a, void* bitset_b, size_t top, size_t start) {
  size_t end = start + 1;
  if (FAL__T(__is_free)(bitset_a, bitset_b, start)) {
    while (end < top && FAL__T(__is_free)(bitset_a, bitset_b, end)) {
      end++;
    }
  }
  else {
    while (end < top && FAL__T(__is_guts)(bitset_a, bitset_b, end)) {
      end++;
    }
  }

  return end - start;
}

/******************************************************************************/
/*                                BANANA BANANA                               */
/******************************************************************************/

static inline FAL_ARENA_T* FAL__T(_for)(void* ptr) {
  return (FAL_ARENA_T*)( ((uintptr_t)ptr) & FAL_ARENA__MASK );
}

static inline void FAL__T(_init)(FAL_ARENA_T* arena) {
  assert(((uintptr_t)arena & FAL_ARENA__BLOCK_MASK) == 0
    && "[" FAL_STR(FAL__T(_init)) "] arena is not aligned to its size");
  memset(arena, 0, FAL_ARENA__FIRST * FAL_ARENA_BLOCK_SIZE);
  *FAL__T(__top_ptr)(arena) = FAL_ARENA__FIRST;
}

/******************************************************************************/
/*                                ALLOCATING                                  */
/******************************************************************************/

static inline void* FAL__T(_bumpalloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__T(_bumpalloc)) "] size cannot be zero");
  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;
  uint16_t* top = FAL__T(__top_ptr)(arena);
  if (*top + size >= FAL_ARENA__BLOCKS) {
    return 0;
  }

  void* result = FAL__T(__markalloc)(arena, *top, size);
  *top += size;

  return result;
}

static inline void* FAL__T(_alloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__T(_alloc)) "] size cannot be zero");

  void* mem = FAL__T(_bumpalloc)(arena, size);
  if (mem) {
    return mem;
  }

  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  size_t start = FAL_ARENA__FIRST;
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

  return FAL__T(__markalloc)(arena, start, size);
}

static inline void FAL__T(_free)(void* ptr) {
  if (!ptr) {
    return;
  }

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);

  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  assert(FAL__T(__is_start(bitset_a, bitset_b, start))
    && "[" FAL_STR(FAL__T(_free)) "] expected start of allocation");

  fal_bitset_clear(bitset_a, start);
  fal_bitset_clear(bitset_b, start);

  uint16_t* top = FAL__T(__top_ptr)(arena);
  size_t end = start + 1;
  while (end < *top && FAL__T(__is_guts)(bitset_a, bitset_b, end)) {
    fal_bitset_clear(bitset_a, end);
    fal_bitset_clear(bitset_b, end);
    end++;
  }

  /* Adjust dump allocation pointer. */
  if (end >= *top) {
    *top = start;
  }
}

/******************************************************************************/
/*                                   MARKING                                  */
/******************************************************************************/

static inline void FAL__T(_mark)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_mark)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);
  void* bitset_a = FAL__T(__bitset_a)(arena);
  fal_bitset_set(bitset_a, start);
}

static inline void FAL__T(_unmark)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_unmark)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);
  void* bitset_a = FAL__T(__bitset_a)(arena);
  fal_bitset_clear(bitset_a, start);
}

/******************************************************************************/
/*                                  QUERYING                                  */
/******************************************************************************/

static inline int FAL__T(_used)(void* ptr) {
  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  size_t ix = FAL__T(__ix_for)(ptr);

  return !FAL__T(__is_free)(bitset_a, bitset_b, ix);
}

static inline size_t FAL__T(_size)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_size)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  uint16_t top = *FAL__T(__top_ptr)(arena);
  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  size_t ix = FAL__T(__ix_for)(ptr);

  return FAL__T(__size)(bitset_a, bitset_b, top, ix);
}

static inline int FAL__T(_marked)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_unmark)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);
  void* bitset_a = FAL__T(__bitset_a)(arena);

  return fal_bitset_test(bitset_a, start);
}

/******************************************************************************/
/*                                 ITERATING                                  */
/******************************************************************************/

static inline void* FAL__T(_first)(FAL_ARENA_T* arena) {
    assert(arena && "[" FAL_STR(FAL__T(_first)) "] arena cannot be NULL");
  uint16_t* top = FAL__T(__top_ptr)(arena);

  if (*top == FAL_ARENA__FIRST) {
    return 0;
  }

  return FAL__T(__block)(arena, FAL_ARENA__FIRST);
}

static inline void* FAL__T(_next_noskip)(void* ptr) {
  if (!ptr) {
    return 0;
  }

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  uint16_t *top = FAL__T(__top_ptr)(arena);
  void* bitset_a = FAL__T(__bitset_a)(arena);
  void* bitset_b = FAL__T(__bitset_b)(arena);

  size_t start = FAL__T(__ix_for)(ptr);
  size_t size = FAL__T(__size)(bitset_a, bitset_b, *top, start);

  if (start + size >= *top) {
    return 0;
  }

  return FAL__T(__block)(arena, start + size);
}

static inline void* FAL__T(_next)(void* ptr) {
    do {
        ptr = FAL__T(_next_noskip)(ptr);
    } while (ptr && !FAL__T(_used)(ptr));

    return ptr;
}

#undef FAL__T

#undef FAL_ARENA__BLOCK_POW
#undef FAL_ARENA__POW
#undef FAL_ARENA_SIZE
#undef FAL_ARENA_BLOCK_SIZE
#undef FAL_ARENA__BLOCKS
#undef FAL_ARENA__BITSET_SIZE
#undef FAL_ARENA__BLOCK_MASK
#undef FAL_ARENA__MASK
#undef FAL_ARENA__FIRST
#undef FAL_ARENA_TOTAL
#undef FAL_ARENA__UNUSED_BITS
#undef FAL_ARENA__UNUSED_BYTES

/* Undef compile-time parameters. */
#ifndef FAL_ARENA_DEF_NO_UNDEF
#undef FAL_ARENA_DEF_BLOCK_POW
#undef FAL_ARENA_DEF_POW
#undef FAL_ARENA_DEF_NAME
#endif /* FAL_ARENA_DEF_NO_UNDEF */

#ifdef __cplusplus
}
#endif

#endif