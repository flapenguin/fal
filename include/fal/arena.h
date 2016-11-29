/* Copyright (c) 2016 Andrey Roenko
 * This file is part of fal project which is released under MIT license.
 * See file LICENSE or go to https://opensource.org/licenses/MIT for full
 * license details.
*/
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
    (opt) FAL_ARENA_DEF_HEADER_SIZE - default: 0; size for header stored at
                                      the start of the efective blocks of arena
    (opt) FAL_ARENA_DEF_INCOMPACT - store internal data in the effective blocks,
                                    not in the unused part of bitset

    (opt) FAL_ARENA_DEF_NO_UNDEF  - do not undefined all compile-time parameters

  Compile-time constraints:
    1. FAL_ARENA_DEF_INCOMPACT must be defined or UnusedBits must be enough to
       store unsigned short (2 bytes).
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
        tries arena_bumpalloc first and fallbacks to looking for freed blocks
      int arena_extend(void* ptr, size_t newsize)
        tries to extend/shrink allocation to newsize
      void arena_free(void*)
        frees passed allocation

      void arena_emplace(void*, size_t)
        forcely make allocation of specified size in specified location without
        any assertions
        previously made allocations which intersect with passed pointer should
        be considered invalid
        usefull if you want to move data around, e.g. shift it to start of arena
      void arena_emplace_end(void*)
        forcely set end of all allocations, all blocks starting at passed pointer
        will be considered freed/unused

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
      size_t arena_bsize(void*)
        get size in blocks of allocation
      size_t arena_size(void*)
        get size in bytes of allocation
      arena_t* arena_for(void*)
        get arena used to allocate passed memory
      size_t arena_bumptop(arena_t*)
        get bump allocator position (between arena_BEGIN and arena_END)
        no blocks are allocated above this position
      void* arena_user_hi(arena_t*)
        access hi user memory, see arena_USER_HI_BYTES
      void* arena_user_lo(arena_t*)
        access lo user memory, see arena_USER_LO_BYTES
      void* arena_header(arena_t*)
        assess arena_HEADER_SIZE bytes at the start of arena
      int arena_can_belong(void*)
        checks if memory can possibly belong to arena, i.e. it doesn't lay
        in potential internal service memory (e.g. bitsets)
        for example, 0x10000 cannot belong to 4KiB arena because it will point
        to its internal structures
      int arena_empty(arena_t*)
        checks if arena is empty
      void* arena_mem_start(arena_t*)
        get address of first byte, which can be used for allocation
      void* arena_mem_end(arena_t*)
        get address of byte next to last byte, which can be used for allocation

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
      arena_SIZE           - arena size in bytes
      arena_EFFECTIVE_SIZE - effective size available for allocation
      arena_BLOCK_SIZE     - block size in bytes
      arena_BEGIN          - id of first block available for allocation
      arena_END            - id of last block available for allocation
      arena_TOTAL          - nubmer of blocks available for allocation
                             equals (arena_END - arena_BEGIN)
      arena_USER_LO_BYTES  - number of bytes available for user fata in LO place
      arena_USER_HI_BYTES  - number of bytes available for user fata in HI place
      arena_HEADER_SIZE    - number of bytes used for header

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

/* Public and internal functions helpers. */
#define FAL__PUB(X)             FAL_CONCAT(FAL_ARENA_DEF_NAME, FAL_CONCAT(_, X))
#define FAL__INT(X)             FAL_CONCAT(FAL_ARENA_DEF_NAME, FAL_CONCAT(__, X))

/* Public */
#define FAL__T                      FAL__PUB(t)

#define FAL_ARENA_TOTAL             FAL__PUB(TOTAL)
#define FAL_ARENA_SIZE              FAL__PUB(SIZE)
#define FAL_ARENA_EFFECTIVE_SIZE    FAL__PUB(EFFECTIVE_SIZE)
#define FAL_ARENA_BLOCK_SIZE        FAL__PUB(BLOCK_SIZE)
#define FAL_ARENA_BEGIN             FAL__PUB(BEGIN)
#define FAL_ARENA_END               FAL__PUB(END)
#define FAL_ARENA_USER_LO_BYTES     FAL__PUB(USER_LO_BYTES)
#define FAL_ARENA_USER_HI_BYTES     FAL__PUB(USER_HI_BYTES)
/* Internal */
#define FAL_ARENA__BLOCK_POW        FAL__INT(BLOCK_POW)
#define FAL_ARENA__POW              FAL__INT(POW)
#define FAL_ARENA__BLOCKS           FAL__INT(BLOCKS)
#define FAL_ARENA__BITSET_SIZE      FAL__INT(BITSET_SIZE)
#define FAL_ARENA__BLOCK_MASK       FAL__INT(BLOCK_MASK)
#define FAL_ARENA__MASK             FAL__INT(MASK)
#define FAL_ARENA__UNUSED_BITS      FAL__INT(UNUSED_BITS)
#define FAL_ARENA__UNUSED_BYTES     FAL__INT(UNUSED_BYTES)

#define FAL_ARENA__HEADER_TOP_SIZE  FAL__INT(BUMPTOP_SIZE)
#define FAL_ARENA__HEADER_BLOCKS    FAL__INT(HEADER_BLOCKS)
#define FAL_ARENA__HEADER_BEGIN     FAL__INT(HEADER_BEGIN)
#define FAL_ARENA__HEADER_SIZE      FAL__INT(HEADER_SIZE)

typedef struct FAL__T FAL__T;

enum FAL__INT(defs) {
  FAL_ARENA__BLOCK_POW = FAL_ARENA_DEF_BLOCK_POW,
  FAL_ARENA__POW = FAL_ARENA_DEF_POW,
  FAL_ARENA_SIZE = 1u << FAL_ARENA__POW,
  FAL_ARENA_BLOCK_SIZE = 1u << FAL_ARENA__BLOCK_POW,

#ifdef FAL_ARENA_DEF_INCOMPACT
  FAL_ARENA__HEADER_TOP_SIZE = sizeof(unsigned short),
#else
  FAL_ARENA__HEADER_TOP_SIZE = 0,
#endif

#ifdef FAL_ARENA_DEF_HEADER_SIZE
  FAL_ARENA__HEADER_SIZE = FAL_ARENA__HEADER_TOP_SIZE + FAL_ARENA_DEF_HEADER_SIZE,
#else
  FAL_ARENA__HEADER_SIZE = FAL_ARENA__HEADER_TOP_SIZE,
#endif

  FAL_ARENA__BLOCKS = FAL_ARENA_SIZE / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA__BITSET_SIZE = FAL_ARENA__BLOCKS / CHAR_BIT,
  FAL_ARENA__BLOCK_MASK = FAL_ARENA_SIZE - 1,
  FAL_ARENA__MASK = ~FAL_ARENA__BLOCK_MASK,

  FAL_ARENA__HEADER_BLOCKS = (FAL_ARENA__HEADER_SIZE + FAL_ARENA_BLOCK_SIZE - 1)
    / FAL_ARENA_BLOCK_SIZE,
  FAL_ARENA__HEADER_BEGIN = 2 *   FAL_ARENA__BITSET_SIZE / FAL_ARENA_BLOCK_SIZE,

  FAL_ARENA_BEGIN = FAL_ARENA__HEADER_BEGIN + FAL_ARENA__HEADER_BLOCKS,
  FAL_ARENA_END = FAL_ARENA__BLOCKS,
  FAL_ARENA_TOTAL = FAL_ARENA__BLOCKS - FAL_ARENA_BEGIN,
  FAL_ARENA__UNUSED_BITS = FAL_ARENA_BEGIN,
  FAL_ARENA__UNUSED_BYTES = FAL_ARENA__UNUSED_BITS / CHAR_BIT,

  FAL_ARENA_EFFECTIVE_SIZE = FAL_ARENA_TOTAL * FAL_ARENA_BLOCK_SIZE,

#ifdef FAL_ARENA_DEF_INCOMPACT
  FAL_ARENA_USER_LO_BYTES = FAL_ARENA__UNUSED_BYTES,
#else
  FAL_ARENA_USER_LO_BYTES = FAL_ARENA__UNUSED_BYTES - sizeof(unsigned short),
#endif

  FAL_ARENA_USER_HI_BYTES = FAL_ARENA__UNUSED_BYTES
};

/******************************************************************************/
/*                            FORWARD DECLARATION                             */
/******************************************************************************/
static inline void FAL__INT(asertions)();
static inline int FAL__INT(ix_for)(void* ptr);
static inline void* FAL__INT(block)(FAL__T* arena, int ix);
static inline void* FAL__INT(mark_bs)(FAL__T* arena);
static inline void* FAL__INT(block_bs)(FAL__T* arena);
static inline unsigned short* FAL__INT(top_ptr)(FAL__T* arena);
static inline void* FAL__INT(markalloc)(FAL__T* arena, size_t start, size_t size);
static inline void FAL__INT(adjust_bumptop)(void* mark_bs, void* block_bs,
  unsigned short* top, size_t oldend, size_t end);
static inline int FAL__INT(is_guts)(void* mark_bs, void* block_bs, size_t ix);
static inline int FAL__INT(is_start)(void* mark_bs, void* block_bs, size_t ix);
static inline int FAL__INT(is_free)(void* mark_bs, void* block_bs, size_t ix);
static inline size_t FAL__INT(bsize)(void* mark_bs, void* block_bs,
  size_t top, size_t start);

static inline void FAL__PUB(init)(FAL__T* arena);

static inline FAL__T* FAL__PUB(for)(void* ptr);
static inline int FAL__PUB(used)(void* ptr);
static inline size_t FAL__PUB(size)(void* ptr);
static inline size_t FAL__PUB(bsize)(void* ptr);
static inline int FAL__PUB(marked)(void* ptr);
static inline size_t FAL__PUB(bumptop)(FAL__T* arena);
static inline void* FAL__PUB(user_lo)(FAL__T* arena);
static inline void* FAL__PUB(user_hi)(FAL__T* arena);

static inline void* FAL__PUB(bumpalloc)(FAL__T* arena, size_t size);
static inline void* FAL__PUB(alloc)(FAL__T* arena, size_t size);
static inline int FAL__PUB(extend)(void* ptr, size_t size);
static inline void FAL__PUB(free)(void* ptr);
static inline void FAL__PUB(emplace)(void* where, size_t size);
static inline void FAL__PUB(emplace_end)(void* where);

static inline void FAL__PUB(mark)(void* ptr);
static inline void FAL__PUB(unmark)(void* ptr);
static inline void FAL__PUB(mark_all)(FAL__T* arena, int mark);

static inline void* FAL__PUB(first)(FAL__T* arena);
static inline void* FAL__PUB(first_noskip)(FAL__T* arena);
static inline void* FAL__PUB(next)(void* ptr);
static inline void* FAL__PUB(next_noskip)(void* ptr);

/******************************************************************************/
/*                                INTERNALS                                   */
/******************************************************************************/
static inline void FAL__INT(asertions)() {
#ifndef FAL_ARENA_DEF_INCOMPACT
  /* Ensure there's enough unused bits at the beginning of each bitset
     to store additional data. */
  FAL_STATIC_ASSERT(FAL_ARENA__UNUSED_BITS >= sizeof(unsigned short) * CHAR_BIT);
#endif

  /* Ensure bump allocation positions will wit in unsigned short */
  FAL_STATIC_ASSERT(FAL_ARENA_TOTAL < (unsigned long long)USHRT_MAX);
}

static inline int FAL__INT(ix_for)(void* ptr) {
  uintptr_t block = ((uintptr_t)ptr) & FAL_ARENA__BLOCK_MASK;
  return block >> FAL_ARENA__BLOCK_POW;
}

static inline void* FAL__INT(block)(FAL__T* arena, int ix) {
  return (char*)arena + ix*FAL_ARENA_BLOCK_SIZE;
}

static inline void* FAL__INT(mark_bs)(FAL__T* arena) {
  return arena;
}

static inline void* FAL__INT(block_bs)(FAL__T* arena) {
  return (char*)(void*)arena + FAL_ARENA__BITSET_SIZE;
}

static inline unsigned short* FAL__INT(top_ptr)(FAL__T* arena) {
#ifdef FAL_ARENA_DEF_INCOMPACT
  return (unsigned short*)FAL__INT(block)(arena, FAL_ARENA__HEADER_BEGIN);
#else
  return (unsigned short*)(void*)arena;
#endif
}

static inline void* FAL__INT(markalloc)(FAL__T* arena, size_t start, size_t size) {
  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  fal_bitset_clear(mark_bs, start);
  fal_bitset_set(block_bs, start);

  for (size_t ix = 1; ix < size; ix++) {
    fal_bitset_set(mark_bs, start + ix);
    fal_bitset_clear(block_bs, start + ix);
  }

  return FAL__INT(block)(arena, start);
}

static inline int FAL__INT(is_guts)(void* mark_bs, void* block_bs, size_t ix) {
  return fal_bitset_test(mark_bs, ix) && !fal_bitset_test(block_bs, ix);
}

static inline int FAL__INT(is_start)(void* mark_bs, void* block_bs, size_t ix) {
  FAL_UNUSED(mark_bs);
  return fal_bitset_test(block_bs, ix);
}

static inline int FAL__INT(is_free)(void* mark_bs, void* block_bs, size_t ix) {
  return !fal_bitset_test(mark_bs, ix) && !fal_bitset_test(block_bs, ix);
}

static inline size_t FAL__INT(bsize)(void* mark_bs, void* block_bs,
  size_t top, size_t start) {
  size_t end = start + 1;
  if (FAL__INT(is_free)(mark_bs, block_bs, start)) {
    while (end < top && FAL__INT(is_free)(mark_bs, block_bs, end)) {
      end++;
    }

    if (end >= top && top != FAL_ARENA_END) {
      end = FAL_ARENA_END;
    }
  }
  else {
    while (end < top && FAL__INT(is_guts)(mark_bs, block_bs, end)) {
      end++;
    }
  }

  return end - start;
}

/******************************************************************************/
/*                              INITIALIZATION                                */
/******************************************************************************/

static inline void FAL__PUB(init)(FAL__T* arena) {
  assert(((uintptr_t)arena & FAL_ARENA__BLOCK_MASK) == 0
    && "[" FAL_STR(FAL__PUB(init)) "] arena is not aligned to its size");

  char* mark_bs = FAL__INT(mark_bs)(arena);
  char* block_bs = FAL__INT(block_bs)(arena);

  memset(mark_bs + FAL_ARENA__UNUSED_BYTES, 0,
    FAL_ARENA__BITSET_SIZE - FAL_ARENA__UNUSED_BYTES);
  memset(block_bs + FAL_ARENA__UNUSED_BYTES, 0,
    FAL_ARENA__BITSET_SIZE - FAL_ARENA__UNUSED_BYTES);

  *FAL__INT(top_ptr)(arena) = FAL_ARENA_BEGIN;
}

/******************************************************************************/
/*                                  QUERYING                                  */
/******************************************************************************/
static inline FAL__T* FAL__PUB(for)(void* ptr) {
  return (FAL__T*)( ((uintptr_t)ptr) & FAL_ARENA__MASK );
}

static inline int FAL__PUB(can_belong)(void* ptr) {
  return !!(((uintptr_t)ptr) & FAL_ARENA__BLOCK_MASK);
}

static inline int FAL__PUB(empty)(FAL__T* arena) {
  return *FAL__INT(top_ptr)(arena) == FAL_ARENA_BEGIN;
}

static inline int FAL__PUB(used)(void* ptr) {
  FAL__T* arena = FAL__PUB(for)(ptr);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  size_t ix = FAL__INT(ix_for)(ptr);

  return !FAL__INT(is_free)(mark_bs, block_bs, ix);
}

static inline size_t FAL__PUB(bsize)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__PUB(bsize)) "] ptr cannot be NULL");

  FAL__T* arena = FAL__PUB(for)(ptr);
  unsigned short top = *FAL__INT(top_ptr)(arena);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  size_t ix = FAL__INT(ix_for)(ptr);

  return FAL__INT(bsize)(mark_bs, block_bs, top, ix);
}

static inline size_t FAL__PUB(size)(void* ptr) {
  return FAL__PUB(bsize)(ptr) * FAL_ARENA_BLOCK_SIZE;
}

static inline int FAL__PUB(marked)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__PUB(marked)) "] ptr cannot be NULL");

  FAL__T* arena = FAL__PUB(for)(ptr);
  size_t start = FAL__INT(ix_for)(ptr);
  void* mark_bs = FAL__INT(mark_bs)(arena);

  return fal_bitset_test(mark_bs, start);
}

static inline size_t FAL__PUB(bumptop)(FAL__T* arena) {
  return *FAL__INT(top_ptr)(arena);
}

static inline void* FAL__PUB(user_lo)(FAL__T* arena) {
  return (char*)FAL__INT(top_ptr)(arena) + sizeof(unsigned short);
}

static inline void* FAL__PUB(user_hi)(FAL__T* arena) {
  return FAL__INT(block_bs)(arena);
}

static inline void* FAL__PUB(header)(FAL__T* arena) {
#ifdef FAL_ARENA_DEF_HEADER_SIZE
  return (void*)((char*)FAL__INT(block)(arena, FAL_ARENA__HEADER_BEGIN)
    + FAL_ARENA__HEADER_TOP_SIZE);
#else
  FAL_UNUSED(arena);
  assert(0 && "[" FAL_STR(FAL__PUB(header)) "] cannot be called, "
    "header doesn't exist for this arena type");
  return 0;
#endif
}

static inline void* FAL__PUB(mem_start)(FAL__T* arena) {
  return FAL__INT(block)(arena, FAL_ARENA_BEGIN);
}

static inline void* FAL__PUB(mem_end)(FAL__T* arena) {
  return (char*)arena + FAL_ARENA_SIZE;
}

/******************************************************************************/
/*                                ALLOCATING                                  */
/******************************************************************************/

static inline void* FAL__PUB(bumpalloc)(FAL__T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__PUB(bumpalloc)) "] size cannot be zero");
  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;
  unsigned short* top = FAL__INT(top_ptr)(arena);
  if (*top + size > FAL_ARENA__BLOCKS) {
    return 0;
  }

  void* result = FAL__INT(markalloc)(arena, *top, size);
  *top += size;

  return result;
}

static inline void* FAL__PUB(alloc)(FAL__T* arena, size_t size) {
  assert(size != 0 && "[" FAL_STR(FAL__PUB(alloc)) "] size cannot be zero");

  /* Try faster bumpalloc first. */
  void* mem = FAL__PUB(bumpalloc)(arena, size);
  if (mem) {
    return mem;
  }

  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  size_t start = FAL_ARENA_BEGIN;
  for (; start < FAL_ARENA_TOTAL; ) {
    size_t free = 0;
    for (; free < size; free++) {
      if (fal_bitset_test(mark_bs, start + free)
        || fal_bitset_test(block_bs, start + free)) {
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

  return FAL__INT(markalloc)(arena, start, size);
}

static inline void FAL__PUB(emplace)(void* where, size_t size) {
  FAL__T* arena = FAL__PUB(for)(where);
  size_t start = FAL__INT(ix_for)(where);

  size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  FAL__INT(markalloc)(arena, start, size);
}

static inline void FAL__PUB(emplace_end)(void* where) {
  FAL__T* arena = FAL__PUB(for)(where);
  size_t ix = FAL__INT(ix_for)(where);

  *FAL__INT(top_ptr)(arena) = ix;
}

static inline int FAL__PUB(extend)(void* ptr, size_t newsize) {
  assert(newsize && "[" FAL_STR(FAL__PUB(extend)) "] newsize cannot be 0");
  newsize = (newsize + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;

  FAL__T* arena = FAL__PUB(for)(ptr);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);
  unsigned short* top = FAL__INT(top_ptr)(arena);

  size_t start = FAL__INT(ix_for)(ptr);
  size_t oldsize = FAL__INT(bsize)(mark_bs, block_bs, *top, start);

  /* Leaving allocation as is. */
  if (newsize == oldsize) {
    return 1;
  }

  size_t oldend = start + oldsize;
  size_t newend = start + newsize;

  /* Shrinking allocation. */
  if (newsize < oldsize) {
    for (size_t ix = newend; ix < oldend; ix++) {
      fal_bitset_clear(mark_bs, ix);
      fal_bitset_clear(block_bs, ix);
    }

    FAL__INT(adjust_bumptop)(mark_bs, block_bs, top, oldend, newend);

    return 1;
  }

  /* Extending allocation. */

  /* Definetely not enough place. */
  if (newend > FAL_ARENA_END) {
    return 0;
  }

  /* Check if there not enough free blocks for extension. */
  for (size_t ix = oldend; ix < newend; ix++) {
    if (!FAL__INT(is_free)(mark_bs, block_bs, ix)) {
      return 0;
    }
  }

  for (size_t ix = oldend; ix < newend; ix++) {
    fal_bitset_clear(block_bs, ix);
    fal_bitset_set(mark_bs, ix);
  }

  FAL__INT(adjust_bumptop)(mark_bs, block_bs, top, oldend, newend);

  return 1;
}

static inline void FAL__PUB(free)(void* ptr) {
  if (!ptr) {
    return;
  }

  FAL__T* arena = FAL__PUB(for)(ptr);
  size_t start = FAL__INT(ix_for)(ptr);

  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  assert(FAL__INT(is_start(mark_bs, block_bs, start))
    && "[" FAL_STR(FAL__PUB(free)) "] expected start of allocation");

  fal_bitset_clear(mark_bs, start);
  fal_bitset_clear(block_bs, start);

  unsigned short* top = FAL__INT(top_ptr)(arena);
  size_t end = start + 1;
  while (end < *top && FAL__INT(is_guts)(mark_bs, block_bs, end)) {
    fal_bitset_clear(mark_bs, end);
    fal_bitset_clear(block_bs, end);
    end++;
  }

  if (end < *top) {
    return;
  }

  FAL__INT(adjust_bumptop)(mark_bs, block_bs, top, *top, end);
}

static inline void FAL__INT(adjust_bumptop)(void* mark_bs, void* block_bs,
  unsigned short* top, size_t oldend, size_t end) {
  if (oldend < *top) {
    return;
  }

  if (end > *top) {
    *top = end;
    return;
  }

  unsigned short new_top = end;
  while (new_top > FAL_ARENA_BEGIN
    && FAL__INT(is_free)(mark_bs, block_bs, new_top - 1)) {
    new_top--;
  }

  *top = new_top;
}

/******************************************************************************/
/*                                   MARKING                                  */
/******************************************************************************/

static inline void FAL__PUB(mark)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__PUB(mark)) "] ptr cannot be NULL");

  FAL__T* arena = FAL__PUB(for)(ptr);
  size_t start = FAL__INT(ix_for)(ptr);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  fal_bitset_set(mark_bs, start);
}

static inline void FAL__PUB(unmark)(void* ptr) {
  assert(ptr && "[" FAL_STR(FAL__PUB(unmark)) "] ptr cannot be NULL");

  FAL__T* arena = FAL__PUB(for)(ptr);
  size_t start = FAL__INT(ix_for)(ptr);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  fal_bitset_clear(mark_bs, start);
}

static inline void FAL__PUB(mark_all)(FAL__T* arena, int mark) {
  assert(arena && "[" FAL_STR(FAL__PUB(mark_all)) "] arena cannot be NULL");

  /* TODO: optimize, get bitsets here, make internal arena_mark and arena_next. */
  for (void* ptr = FAL__PUB(first)(arena); ptr; ptr = FAL__PUB(next)(ptr)) {
    if (mark) {
      FAL__PUB(mark)(ptr);
    } else {
      FAL__PUB(unmark)(ptr);
    }
  }
}

/******************************************************************************/
/*                                 ITERATING                                  */
/******************************************************************************/

static inline void* FAL__PUB(first)(FAL__T* arena) {
  void* ptr = FAL__PUB(first_noskip)(arena);

  return FAL__PUB(used)(ptr) ? ptr : FAL__PUB(next)(ptr);
}

static inline void* FAL__PUB(first_noskip)(FAL__T* arena) {
  assert(arena && "[" FAL_STR(FAL__PUB(first)) "] arena cannot be NULL");

  return FAL__INT(block)(arena, FAL_ARENA_BEGIN);
}

static inline void* FAL__PUB(next)(void* ptr) {
  do {
      ptr = FAL__PUB(next_noskip)(ptr);
  } while (ptr && !FAL__PUB(used)(ptr));

  return ptr;
}

static inline void* FAL__PUB(next_noskip)(void* ptr) {
  if (!ptr) {
    return 0;
  }

  FAL__T* arena = FAL__PUB(for)(ptr);
  unsigned short *top = FAL__INT(top_ptr)(arena);
  void* mark_bs = FAL__INT(mark_bs)(arena);
  void* block_bs = FAL__INT(block_bs)(arena);

  size_t start = FAL__INT(ix_for)(ptr);
  size_t size = FAL__INT(bsize)(mark_bs, block_bs, *top, start);

  if (start + size >= FAL_ARENA_END) {
    return 0;
  }

  return FAL__INT(block)(arena, start + size);
}

#undef FAL__PUB
#undef FAL__INT
#undef FAL__T

#undef FAL_ARENA_SIZE
#undef FAL_ARENA_BLOCK_SIZE
#undef FAL_ARENA_BEGIN
#undef FAL_ARENA_END
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
#undef FAL_ARENA__HEADER_BLOCKS
#undef FAL_ARENA__HEADER_BEGIN
#undef FAL_ARENA__HEADER_TOP_SIZE
#undef FAL_ARENA__HEADER_SIZE

/* Undef compile-time parameters. */
#ifndef FAL_ARENA_DEF_NO_UNDEF
#undef FAL_ARENA_DEF_BLOCK_POW
#undef FAL_ARENA_DEF_POW
#undef FAL_ARENA_DEF_NAME

#ifdef FAL_ARENA_DEF_HEADER_SIZE
#undef FAL_ARENA_DEF_HEADER_SIZE
#endif

#ifdef FAL_ARENA_DEF_INCOMPACT
#undef FAL_ARENA_DEF_INCOMPACT
#endif
#endif /* FAL_ARENA_DEF_NO_UNDEF */

#ifdef __cplusplus
}
#endif

#endif
