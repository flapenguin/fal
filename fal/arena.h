#ifndef __FAL_ARENA_H__
#define __FAL_ARENA_H__

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "bitset.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FAL_ARENA_BLOCK_POW) || !defined(FAL_ARENA_POW)
#error FAL_ARENA_BLOCK_POW and FAL_ARENA_POW must be defined.
#endif

/*
    Arena layout for 16 KiB arena with 16 byte blocks.
        XXXXYYYY AAAA~~~~AAAA ZZZZZZZZ BBBB~~~~BBBB OOOO~~~~OOOO

        16 KiB arena is split into 4096 blocks of 16 bytes each.
        First 64 blocks = 1024 bytes reserved for two bitmasks A and B
        (32 blocks = 512 bytes each) described below.
        At start of each bitmask 8 bytes remain unused and used for X, Y and Z data.
        Remaining 4032 blocks O are available for allocation.

        Bitmasks A and B contain single bit per block each with next meaning.
        A | B
        0   0 Block is not allocated.
        0   1 Block is allocated and flag is unset.
        1   0 Block is a continuation of allocation.
        1   1 Block is allocated and flag is set.

        X space is used to store ix of first free block.
        Y and Z space is used for user data.
*/

typedef struct fal_arena_t fal_arena_t;

#define FAL_ARENA_SIZE        (1u << FAL_ARENA_POW)
#define FAL_ARENA_BLOCK_SIZE  (1u << FAL_ARENA_BLOCK_POW)

#define FAL_ARENA_BLOCKS      (FAL_ARENA_SIZE / FAL_ARENA_BLOCK_SIZE)
#define FAL_ARENA_BITSET_SIZE (FAL_ARENA_BLOCKS / CHAR_BIT)

#define FAL_ARENA_BLOCK_MASK  (FAL_ARENA_SIZE - 1)
#define FAL_ARENA_MASK        (~FAL_ARENA_BLOCK_MASK)
#define FAL_ARENA_FIRST       (2*FAL_ARENA_BITSET_SIZE / FAL_ARENA_BLOCK_SIZE)
#define FAL_ARENA_TOTAL       (FAL_ARENA_BLOCKS - FAL_ARENA_FIRST)

#define FAL_ARENA_UNUSED_BITS   FAL_ARENA_FIRST
#define FAL_ARENA_UNUSED_BYTES  (FAL_ARENA_BITSET_SIZE / CHAR_BIT)


static_assert(FAL_ARENA_UNUSED_BITS >= CHAR_BIT * 2,
    "Not enough unused bits (increase FAL_ARENA_POW or decrease FAL_ARENA_BLOCK_POW)");

static inline int fal_arena__ix_for(void* ptr) {
    uintptr_t block = ((uintptr_t)ptr) & FAL_ARENA_MASK;
    return block >> FAL_ARENA_BLOCK_POW;
}

static inline void* fal_arena__block(fal_arena_t* arena, int ix) {
    return (char*)arena + ix*FAL_ARENA_BLOCK_SIZE;
}

static inline void* fal_arena__bitset_a(fal_arena_t* arena) {
    return arena;
}

static inline void* fal_arena__bitset_b(fal_arena_t* arena) {
    return (char*)(void*)arena + FAL_ARENA_BITSET_SIZE;
}

static inline uint16_t* fal_arena__top_ptr(fal_arena_t* arena) {
    return (uint16_t*)(void*)arena;
}

static inline fal_arena_t* fal_arena_for(void* ptr) {
    uintptr_t x = ((uintptr_t)ptr) & FAL_ARENA_BLOCK_MASK;
    return (fal_arena_t*)x;
}

static inline void fal_arena_init(fal_arena_t* arena) {
    assert(((uintptr_t)arena & FAL_ARENA_BLOCK_MASK) == 0
        && "[fal_arena_init] arena is not aligned to its size");
    *fal_arena__top_ptr(arena) = FAL_ARENA_FIRST;
}

static inline void* fal_arena_alloc(fal_arena_t* arena, size_t size) {
    assert(size != 0 && "[fal_arena_alloc] size cannot be zero");
    size = (size + FAL_ARENA_BLOCK_SIZE - 1) / FAL_ARENA_BLOCK_SIZE;
    uint16_t* top = fal_arena__top_ptr(arena);
    if (*top + size >= FAL_ARENA_BLOCKS) {
        return 0;
    }

    void* bitset_a = fal_arena__bitset_a(arena);
    void* bitset_b = fal_arena__bitset_b(arena);

    fal_bitset_clear(bitset_a, *top);
    fal_bitset_set(bitset_b, *top);

    for (size_t ix = 1; ix < size; ix++) {
        fal_bitset_set(bitset_a, *top + ix);
        fal_bitset_clear(bitset_b, *top + ix);
    }

    void* result = fal_arena__block(arena, size);
    *top += size;

    return result;
}


#ifdef __cplusplus
}
#endif

#endif