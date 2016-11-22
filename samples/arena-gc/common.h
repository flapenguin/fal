#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <stdio.h>

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      space
#include <fal/arena.h>

static inline void* mkspace();

#if defined(_WIN32)
# define __VC_EXTRALEAN
# include <Windows.h>
  static inline void* mkspace() {
    return VirtualAlloc(0, space_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  }
#elif defined(linux) || defined(__MINGW32__) || defined(__GNUC__)
# include <sys/mman.h>
  static inline void* mkspace() {
    return mmap(0, space_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }
#else
# error Dont know how to alloc page on this system.
#endif

typedef struct object_t object_t;
struct object_t {
  int id;
  struct object_t* next;
};

static void print_objs(space_t* space) {
  for (object_t* obj = space_first_noskip(space); obj; obj = space_next_noskip(obj)) {
    if (space_used(obj)) {
      printf("  @%p: id:%#-2x next:%p%s\n", (void*)obj, obj->id, (void*)obj->next,
        space_marked(obj) ? " (marked)" : "");
    } else {
      printf("  @%p %zu empty bytes\n", (void*)obj, space_size(obj));
    }
  }
}

static void alloc_objs(space_t* space, object_t* objs[], size_t len) {
  for (size_t i = 0; i < len; i++) {
    objs[i] = space_bumpalloc(space, sizeof(object_t));
    objs[i]->id = 0x100 + i;
  }

  for (size_t i = 0; i < len; i++) {
    objs[i]->next = i < len - 2 ? objs[i + 2] : 0;
  }
}

#endif /* __COMMON_H__ */
