#ifndef __FAL_TEST_ARENA_TESTLIB_H__
#define __FAL_TEST_ARENA_TESTLIB_H__

#include <stddef.h>
#include <assert.h>

static inline void* testlib_alloc_arena(size_t size);

#if defined(_WIN32)

#define __VC_EXTRALEAN
#include <Windows.h>
#undef min
#undef max

static inline void* testlib_alloc_arena(size_t size) {
  void* mem = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  assert(mem && "VirtualAlloc");

  return mem;
}

#elif defined(linux) || defined(__MINGW32__) || defined(__GNUC__)

#include <sys/mman.h>

static inline void* testlib_alloc_arena(size_t size) {
  void* mem = mmap(0, size,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS,
    -1,
    0);
  assert(mem && "mmap");

  return mem;
}

#else

#error Dont know how to mmap on this system.

#endif /* defined(_WIN32) */



#endif /* __FAL_TEST_MMAP_H__ */
