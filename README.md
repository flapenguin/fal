# fal - flapenguin's algorithm library for c99

This is header-only library.
Just add `include` directory to you include path and enjoy.

## `fal/arena.h`
Generic arena, i.e. memory block aligned to its size split into small blocks
available for allocation. Because arena is aligned to its size arena's address
can be determined from address of allocation.
Only 1/64th of this memory is used for metadata, so overhead is around 1.5%.
Can store additional bit per allocation called `mark`.

See header comment in `fal/arena.h` for docs.

Based on [LuaJIT arenas](http://wiki.luajit.org/New-Garbage-Collector#arenas).

Basic usage:
```c
#define FAL_ARENA_DEF_NAME      arena /* prefix */
#define FAL_ARENA_DEF_POW       16u   /* 64 KiB arena */
#define FAL_ARENA_DEF_BLOCK_POW 4u    /* 16 byte blocks */
#include <fal/arena.h>

arena_t* a = mmap(arena_SIZE, ...);
arena_init(a)

/* store additional arena_USER_LO_BYTES and arena_USER_HI_BYTES  */
(int*)arena_get_user_lo(a) = 777;
*(my_header*)arena_get_user_hi(a) = (my_header){.name = "!!!", .next = 0};

/* alloc 42 bytes at the end, fail if overflows */
void* x = arena_bumpalloc(a, 42);
/* allocate 37 bytes, search for free cells if no palce at the end */
void* y = arena_alloc(a, 37);

arena_free(x);        /* free previously allocated memory */

/* try to extend allocation to specified size, e.g. add 32 bytes */
if (!arena_extend(x, arena_size(x) + 32)) {
  printf("failed\n");
}

arena_mark(x);        /* set additional bit for allocation */
arena_unmark(x);      /* clear additional bit for allocation */

arena_mark_all(a, 1); /* set bit for all allocations */
arena_mark_all(a, 0); /* clear bit for all allocations */

/* iterate through allocations */
for (void* p = arena_first(a); p; p = arena_next(p)) {
  printf("@%p size=%zu marked=%d\n", p,
    arena_size(p),    /* get size of an allocation */
    arena_marked(p)); /* get additional bit for an allocation*/
}

/* iterate through allocations and gaps between them */
for (void* p = arena_first_noskip(a); p; p = arena_next_noskip(p)) {
  printf("@%p used=%d size=%zu marked=%d\n", p,
    arena_used(p),    /* check if p points to an actual allocation or a gap */
    arena_size(p),    /* get size of an allocation */
    arena_marked(p)); /* get additional bit for an allocation*/
}
```

## `fal/bitset.h`

Bitset helpers.

```c
char bs[fal_bitset_size(77)];                /* create bitset with 77 bit */
fal_bitset_set(bs, 42)                       /* set bit #42 */
fal_bitset_clear(bs, 42)                     /* clear bit #42 */
fal_bitset_assign(bs, 42, my_bool)           /* set/clear bit #42 */
putchar(fal_bitset_test(bs, 42) ? 'x' : 'o') /* check if bit #42 is set */
```

## `fal/utils.h`

```c
/* fail compilation if condition is not met */
FAL_STATIC_ASSERT(some_enum_constant >= sizeof(unsigned short));
```

## Running tests
1. `cd build`
2. `cmake path/to/fal` (specify any options you need)
3. `make`
4. `ctest`
