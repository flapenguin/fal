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

    (opt) FAL_ARENA_DEF_NO_UNDEF  - do not undefined all compile-time parameters

  Compile-time constraints:
    1. UnusedBits must be enough to store unsigned short (2 bytes).
                            2 * ArenaSize
          UnusedBits = ----------------------
                       CHAR_BIT * BlockSize^2

  Run-time constraints:
    1. Arena must be aligned to it size.

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
      void arena_mark_all(arena_t*, int marked)
        mark/unmark all blocks

    Querying:
      int arena_used(void*)
        check if memory is allocated
      size_t arena_size(void*)
        get size (in blocks) of allocation
      arena_t* arena_for(void*)
        get arena used to allocate passed memory
      unsigned int arena_bumptop(arena_t*)
        get bump allocator position (between arena_FIRST and arena_LAST)
        no blocks are allocated above this position
      void* arena_user_hi(arena_t*)
        access hi user memory, see arena_USER_HI_BYTES
      void* arena_user_lo(arena_t*)
        access lo user memory, see arena_USER_LO_BYTES

    Iterating:
      void* arena_first(arena_t*)
        get first allocation
      void* arena_first_noskip(arena_t*)
        get first allocation, including freed blocks (use arena_used)
      void* arena_next(void*)
        get next allocation, skipping freed blocks
      void* arena_next_noskip(void*)
        get next allocation, including freed blocks (use arena_used)

    Constants:
      arena_SIZE          - arena size in bytes
      arena_BLOCK_SIZE    - block size in bytes
      arena_FIRST         - id of first block available for allocation
      arena_LAST          - id of last block available for allocation
      arena_TOTAL         - nubmer of blocks available for allocation
                            equals (arena_LAST - arena_FIRST)
      arena_USER_LO_BYTES - number of bytes available for user fata in LO place
      arena_USER_HI_BYTES - number of bytes available for user fata in HI place

  Arena layout example is for 16 KiB arena with 16 byte blocks.
    XXXXYYYY MMMM~~~~MMMM ZZZZZZZZ BBBB~~~~BBBB OOOO~~~~OOOO

    16 KiB arena is split into 4096 blocks of 16 bytes each.
    First 64 blocks = 1024 bytes reserved for two bitmasks M and B
    (32 blocks = 512 bytes each) described below.

    At start of each bitmask 8 bytes remain unused and used for X, Y and Z data.
    Remaining 4032 blocks O are available for allocation.

    M = mark bitset
    B = block bitset
    Both bitsets contain single bit per block.

      Block Mark
        0    0   Free block.
        0    1   Allocation extension.
        1    0   Start of allocation, flag is unset.
        1    1   Start of allocation, flag is set.

    X space is used to store ix of first free block in unsigned short (2 bytes).

    Y and Z space is used for user data:
      Y = user LO bytes
      Z = user HI bytes
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "utils.h"
#include "bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FAL_ARENA_DEF_BLOCK_POW) \
  || !defined(FAL_ARENA_DEF_POW) \
  || !defined(FAL_ARENA_DEF_NAME)
#error FAL_ARENA: compile-time parameters \
  FAL_ARENA_DEF_BLOCK_POW, FAL_ARENA_DEF_POW and FAL_ARENA_DEF_NAME \
  must be defined.
#endif

#define FAL__T(X)               FAL_CONCAT(FAL_ARENA_DEF_NAME, X)

/* Public */
#define FAL_ARENA_T             FAL__T(_t)

#define FAL_ARENA_TOTAL         FAL__T(_TOTAL)
#define FAL_ARENA_SIZE          FAL__T(_SIZE)
#define FAL_ARENA_BLOCK_SIZE    FAL__T(_BLOCK_SIZE)
#define FAL_ARENA_FIRST         FAL__T(_FIRST)
#define FAL_ARENA_LAST          FAL__T(_LAST)
#define FAL_ARENA_USER_LO_BYTES FAL__T(_USER_LO_BYTES)
#define FAL_ARENA_USER_HI_BYTES FAL__T(_USER_HI_BYTES)
/* Internal */
#define FAL_ARENA__BLOCK_POW    FAL__T(__BLOCK_POW)
#define FAL_ARENA__POW          FAL__T(__POW)
#define FAL_ARENA__BLOCKS       FAL__T(__BLOCKS)
#define FAL_ARENA__BITSET_SIZE  FAL__T(__BITSET_SIZE)
#define FAL_ARENA__BLOCK_MASK   FAL__T(__BLOCK_MASK)
#define FAL_ARENA__MASK         FAL__T(__MASK)
#define FAL_ARENA__UNUSED_BITS  FAL__T(__UNUSED_BITS)
#define FAL_ARENA__UNUSED_BYTES FAL__T(__UNUSED_BYTES)

typedef struct FAL_ARENA_T FAL_ARENA_T;

enum FAL__T(__defs) {
  FAL_ARENA__BLOCK_POW = FAL_ARENA_DEF_BLOCK_POW,
  FAL_ARENA__POW = FAL_ARENA_DEF_POW,
  FAL_ARENA_SIZE = 1u << FAL_ARENA__POW,
  FAL_ARENA_BLOCK_SIZE = 1u << FAL_ARENA__BLOCK_POW,
  FAL_ARENA__BLOCKS = FAL_ARENA_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA__BITSET_SIZE = FAL_ARENA__BLOCKS / CHAR_BIT,
  FAL_ARENA__BLOCK_MASK = FAL_ARENA_SIZE - 1,
  FAL_ARENA__MASK = ~FAL_ARENA__BLOCK_MASK,
  FAL_ARENA_FIRST = 2 * FAL_ARENA__BITSET_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA_LAST = FAL_ARENA__BLOCKS,
  FAL_ARENA_TOTAL = FAL_ARENA__BLOCKS - FAL_ARENA_FIRST,
  FAL_ARENA__UNUSED_BITS = FAL_ARENA_FIRST,
  FAL_ARENA__UNUSED_BYTES = FAL_ARENA__UNUSED_BITS / CHAR_BIT,

  FAL_ARENA_USER_LO_BYTES = FAL_ARENA__UNUSED_BYTES - sizeof(unsigned short),
  FAL_ARENA_USER_HI_BYTES = FAL_ARENA__UNUSED_BYTES
};

/******************************************************************************/
/*                            FORWARD DECLARATION                             */
/******************************************************************************/
static inline void FAL__T(__asertions)();
static inline int FAL__T(__ix_for)(void* ptr);
static inline void* FAL__T(__block)(FAL_ARENA_T* arena, int ix);
static inline void* FAL__T(__mark_bs)(FAL_ARENA_T* arena);
static inline void* FAL__T(__block_bs)(FAL_ARENA_T* arena);
static inline unsigned short* FAL__T(__top_ptr)(FAL_ARENA_T* arena);
static inline void* FAL__T(__markalloc)(FAL_ARENA_T* arena, size_t start, size_t size);
static inline int FAL__T(__is_guts)(void* mark_bs, void* block_bs, size_t ix);
static inline int FAL__T(__is_start)(void* mark_bs, void* block_bs, size_t ix);
static inline int FAL__T(__is_free)(void* mark_bs, void* block_bs, size_t ix);
static inline size_t FAL__T(__size)(void* mark_bs, void* block_bs, size_t top, size_t start);

static inline void FAL__T(_init)(FAL_ARENA_T* arena);

static inline FAL_ARENA_T* FAL__T(_for)(void* ptr);
static inline int FAL__T(_used)(void* ptr);
static inline size_t FAL__T(_size)(void* ptr);
static inline int FAL__T(_marked)(void* ptr);
static inline unsigned int FAL__T(_bumptop)(FAL_ARENA_T* arena);
static inline void* FAL__T(_user_lo)(FAL_ARENA_T* arena);
static inline void* FAL__T(_user_hi)(FAL_ARENA_T* arena);

static inline void* FAL__T(_bumpalloc)(FAL_ARENA_T* arena, size_t size);
static inline void* FAL__T(_alloc)(FAL_ARENA_T* arena, size_t size);
static inline void FAL__T(_free)(void* ptr);

static inline void FAL__T(_mark)(void* ptr);
static inline void FAL__T(_unmark)(void* ptr);
static inline void FAL__T(_mark_all)(FAL_ARENA_T* arena, int mark);

static inline void* FAL__T(_first)(FAL_ARENA_T* arena);
static inline void* FAL__T(_first_noskip)(FAL_ARENA_T* arena);
static inline void* FAL__T(_next)(void* ptr);
static inline void* FAL__T(_next_noskip)(void* ptr);

/******************************************************************************/
/*                                INTERNALS                                   */
/******************************************************************************/
static inline void FAL__T(__asertions)() {
  /* Ensure there's enough unused bits at the beginning of each bitset
     to store additional data. */
  FAL_STATIC_ASSERT(FAL_ARENA__UNUSED_BITS >= sizeof(unsigned short) * CHAR_BIT);

  /* Ensure bump allocation positions will wit in unsigned short */
  FAL_STATIC_ASSERT(FAL_ARENA_TOTAL < (unsigned long long)USHRT_MAX);
}

static inline int FAL__T(__ix_for)(void* ptr) {
  uintptr_t block = ((uintptr_t)ptr) & FAL_ARENA__BLOCK_MASK;
  return block >> FAL_ARENA__BLOCK_POW;
}

static inline void* FAL__T(__block)(FAL_ARENA_T* arena, int ix) {
  return (char*)arena + ix*FAL_ARENA_BLOCK_SIZE;
}

static inline void* FAL__T(__mark_bs)(FAL_ARENA_T* arena) {
  return arena;
}

static inline void* FAL__T(__block_bs)(FAL_ARENA_T* arena) {
  return (char*)(void*)arena + FAL_ARENA__BITSET_SIZE;
}

static inline unsigned short* FAL__T(__top_ptr)(FAL_ARENA_T* arena) {
  return (unsigned short*)(void*)arena;
}

static inline void* FAL__T(__markalloc)(FAL_ARENA_T* arena, size_t start, size_t size) {
  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  fal_bitset_clear(mark_bs, start);
  fal_bitset_set(block_bs, start);

  for (size_t ix = 1; ix < size; ix++) {
    fal_bitset_set(mark_bs, start + ix);
    fal_bitset_clear(block_bs, start + ix);
  }

  return FAL__T(__block)(arena, start);
}

static inline int FAL__T(__is_guts)(void* mark_bs, void* block_bs, size_t ix) {
  return fal_bitset_test(mark_bs, ix) && !fal_bitset_test(block_bs, ix);
}

static inline int FAL__T(__is_start)(void* mark_bs, void* block_bs, size_t ix) {
  return fal_bitset_test(block_bs, ix);
}

static inline int FAL__T(__is_free)(void* mark_bs, void* block_bs, size_t ix) {
  return !fal_bitset_test(mark_bs, ix) && !fal_bitset_test(block_bs, ix);
}

static inline size_t FAL__T(__size)(void* mark_bs, void* block_bs, size_t top, size_t start) {
  size_t end = start + 1;
  if (FAL__T(__is_free)(mark_bs, block_bs, start)) {
    while (end < top && FAL__T(__is_free)(mark_bs, block_bs, end)) {
      end++;
    }

    if (end >= top && top != FAL_ARENA_LAST) {
      end = FAL_ARENA_LAST;
    }
  }
  else {
    while (end < top && FAL__T(__is_guts)(mark_bs, block_bs, end)) {
      end++;
    }
  }

  return end - start;
}

/******************************************************************************/
/*                              INITIALIZATION                                */
/******************************************************************************/

static inline void FAL__T(_init)(FAL_ARENA_T* arena) {
  assert(((uintptr_t)arena & FAL_ARENA__BLOCK_MASK) == 0
    && "[" FAL_STR(FAL__T(_init)) "] arena is not aligned to its size");
  memset(arena, 0, FAL_ARENA_FIRST * FAL_ARENA_BLOCK_SIZE);
  *FAL__T(__top_ptr)(arena) = FAL_ARENA_FIRST;
}

/******************************************************************************/
/*                                  QUERYING                                  */
/******************************************************************************/

static inline FAL_ARENA_T* FAL__T(_for)(void* ptr) {
  return (FAL_ARENA_T*)( ((uintptr_t)ptr) & FAL_ARENA__MASK );
}

static inline int FAL__T(_used)(void* ptr) {
  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  size_t ix = FAL__T(__ix_for)(ptr);

  return !FAL__T(__is_free)(mark_bs, block_bs, ix);
}

static inline size_t FAL__T(_size)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_size)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  unsigned short top = *FAL__T(__top_ptr)(arena);
  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  size_t ix = FAL__T(__ix_for)(ptr);

  return FAL__T(__size)(mark_bs, block_bs, top, ix);
}

static inline int FAL__T(_marked)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_marked)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);
  void* mark_bs = FAL__T(__mark_bs)(arena);

  return fal_bitset_test(mark_bs, start);
}

static inline unsigned int FAL__T(_bumptop)(FAL_ARENA_T* arena) {
  return *FAL__T(__top_ptr)(arena);
}

static inline void* FAL__T(_user_lo)(FAL_ARENA_T* arena) {
  return (char*)FAL__T(__top_ptr)(arena) + sizeof(unsigned short);
}

static inline void* FAL__T(_user_hi)(FAL_ARENA_T* arena) {
  return FAL__T(__block_bs)(arena);
}

/******************************************************************************/
/*                                ALLOCATING                                  */
/******************************************************************************/

static inline void* FAL__T(_bumpalloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__T(_bumpalloc)) "] size cannot be zero");
  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;
  unsigned short* top = FAL__T(__top_ptr)(arena);
  if (*top + size >= FAL_ARENA__BLOCKS) {
    return 0;
  }

  void* result = FAL__T(__markalloc)(arena, *top, size);
  *top += size;

  return result;
}

static inline void* FAL__T(_alloc)(FAL_ARENA_T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__T(_alloc)) "] size cannot be zero");

  /* Try faster bumpalloc first. */
  void* mem = FAL__T(_bumpalloc)(arena, size);
  if (mem) {
    return mem;
  }

  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  size_t start = FAL_ARENA_FIRST;
  for (; start < FAL_ARENA_TOTAL; ) {
    size_t free = 0;
    for (; free < size; free++) {
      if (fal_bitset_test(mark_bs, start + free) || fal_bitset_test(block_bs, start + free)) {
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

  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  assert(FAL__T(__is_start(mark_bs, block_bs, start))
    && "[" FAL_STR(FAL__T(_free)) "] expected start of allocation");

  fal_bitset_clear(mark_bs, start);
  fal_bitset_clear(block_bs, start);

  unsigned short* top = FAL__T(__top_ptr)(arena);
  size_t end = start + 1;
  while (end < *top && FAL__T(__is_guts)(mark_bs, block_bs, end)) {
    fal_bitset_clear(mark_bs, end);
    fal_bitset_clear(block_bs, end);
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
  void* mark_bs = FAL__T(__mark_bs)(arena);
  fal_bitset_set(mark_bs, start);
}

static inline void FAL__T(_unmark)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__T(_unmark)) "] ptr cannot be NULL");

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  size_t start = FAL__T(__ix_for)(ptr);
  void* mark_bs = FAL__T(__mark_bs)(arena);
  fal_bitset_clear(mark_bs, start);
}

static inline void FAL__T(_mark_all)(FAL_ARENA_T* arena, int mark) {
  assert(arena && "[" FAL_STR(FAL__T(_mark_all)) "] arena cannot be NULL");

  // TODO: optimize, get bitsets here, make internal arena_mark and arena_next.
  for (void* ptr = FAL__T(_first)(arena); ptr; ptr = FAL__T(_next)(ptr)) {
    if (mark) {
      FAL__T(_mark)(ptr);
    } else {
      FAL__T(_unmark)(ptr);
    }
  }
}

/******************************************************************************/
/*                                 ITERATING                                  */
/******************************************************************************/

static inline void* FAL__T(_first)(FAL_ARENA_T* arena) {
  void* ptr = FAL__T(_first_noskip)(arena);

  return FAL__T(_used)(ptr) ? ptr : FAL__T(_next)(ptr);
}

static inline void* FAL__T(_first_noskip)(FAL_ARENA_T* arena) {
  assert(arena && "[" FAL_STR(FAL__T(_first)) "] arena cannot be NULL");

  return FAL__T(__block)(arena, FAL_ARENA_FIRST);
}

static inline void* FAL__T(_next)(void* ptr) {
    do {
        ptr = FAL__T(_next_noskip)(ptr);
    } while (ptr && !FAL__T(_used)(ptr));

    return ptr;
}

static inline void* FAL__T(_next_noskip)(void* ptr) {
  if (!ptr) {
    return 0;
  }

  FAL_ARENA_T* arena = FAL__T(_for)(ptr);
  unsigned short *top = FAL__T(__top_ptr)(arena);
  void* mark_bs = FAL__T(__mark_bs)(arena);
  void* block_bs = FAL__T(__block_bs)(arena);

  size_t start = FAL__T(__ix_for)(ptr);
  size_t size = FAL__T(__size)(mark_bs, block_bs, *top, start);

  if (start + size > *top) {
    return 0;
  }

  return FAL__T(__block)(arena, start + size);
}

#undef FAL__T

#undef FAL_ARENA_SIZE
#undef FAL_ARENA_BLOCK_SIZE
#undef FAL_ARENA_FIRST
#undef FAL_ARENA_LAST
#undef FAL_ARENA_TOTAL
#undef FAL_ARENA_USER_LO_BYTES
#undef FAL_ARENA_USER_HI_BYTES

#undef FAL_ARENA__BLOCK_POW
#undef FAL_ARENA__POW
#undef FAL_ARENA__BLOCKS
#undef FAL_ARENA__BITSET_SIZE
#undef FAL_ARENA__BLOCK_MASK
#undef FAL_ARENA__MASK
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
