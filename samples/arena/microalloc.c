
#include <stdio.h>
#include <string.h>

/*
  mc_alloc is small and dirty alloc/free/realloc implementation.
  It distinguishes small (< ~1000 bytes) and huge allocation and uses fal/arena
  for first ones.


  For small allocations (< 1000 bytes) mc_alloc uses linked list of
  arenas called buckets (mc_bucket_t):
   START BUCKET BUCKET BUCKET ... BUCKET  NULL
    \----/   \---/  \---/  \--...--/  \---/

  During mc_alloc it walk though them until it finds one that have enough free
  memory and allocates from it. If none was found it allocates new page from os
  and appends it to end of list.

  For huge allocations mc_alloc allocates memory directly from os.

  Microalloc can distinguish small allocations from huge by calling
  arena_can_belong, since huge allocation aligned at start of possible
  bucket they cannot be possibly allocated from bucket.

  For small allocations mc_free frees memory in bucket and removes bucket from
  list and returns it to os if it became empty after removal.
  For huge allocations mc_free simply returns memory to os.

  mc_realloc tries to use arena_extend for small allocations and fallbacks to
  mc_alloc-memcpy-mc_free if it doesn't work and for all huge allocations.
*/

#if defined(_WIN32)
# define __VC_EXTRALEAN
# include <Windows.h>
  static inline void* osalloc(size_t size) {
    void* mem = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return fprintf(stderr, "[trace] VirtualAlloc: %p\n", mem), mem;
  }
#elif defined(linux) || defined(__MINGW32__) || defined(__GNUC__)
# include <sys/mman.h>
  static inline void* osalloc(size_t size) {
    void* mem = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return fprintf(stderr, "[trace] mmap: %p\n", mem), mem;
  }
  static inline void osfree(void* ptr, size_t size) {
    munmap(ptr, size);
    fprintf(stderr, "[trace] munmap: %p %zu\n", ptr, size);
  }
#else
# error Dont know how to alloc page on this system.
#endif

typedef struct mc_header_t mc_header_t;
struct mc_header_t {
  mc_header_t* next;
  mc_header_t* prev;
};

typedef struct mc_huge_t mc_huge_t;
struct mc_huge_t {
  void* ptr;
  size_t size;
};

#define FAL_ARENA_DEF_POW         12u /* 4 KiB arena */
#define FAL_ARENA_DEF_BLOCK_POW   3u  /* 8 byte blocks */
#define FAL_ARENA_DEF_INCOMPACT   /* 4 KiB arena can't be compact */
#define FAL_ARENA_DEF_HEADER_SIZE sizeof(mc_header_t)
#define FAL_ARENA_DEF_NAME        mc_bucket
#include <fal/arena.h>

static mc_bucket_t* mc_huge = 0;
static mc_header_t* mc_start = 0;
void mc_init() {
  mc_huge = osalloc(mc_bucket_SIZE);
  assert(mc_huge);

  mc_bucket_init(mc_huge);
}

void* mc_alloc(size_t size) {
  if (size > mc_bucket_EFFECTIVE_SIZE / 4) {
    mc_huge_t* entry = mc_bucket_alloc(mc_huge, sizeof(mc_huge_t));
    assert(entry && "No more space for huge entries.");

    void* mem = osalloc(size);
    entry->ptr = mem;
    entry->size = size;

    return mem;
  }

  mc_header_t* last = 0;
  mc_header_t* curr = mc_start;
  for (; curr; last = curr, curr = curr->next) {
    mc_bucket_t* bucket = mc_bucket_for(curr);
    void* mem = mc_bucket_alloc(bucket, size);
    if (mem) {
      return mem;
    }
  }

  mc_bucket_t* bucket = osalloc(mc_bucket_SIZE);
  mc_bucket_init(bucket);

  mc_header_t* header = mc_bucket_header(bucket);
  header->next = 0;
  header->prev = last;

  if (last) {
    last->next = header;
  } else {
    mc_start = header;
  }


  return mc_bucket_alloc(bucket, size);
}

mc_huge_t* mc__get_huge(void* ptr) {
  for (mc_huge_t* entry = mc_bucket_first(mc_huge); entry; entry = mc_bucket_next(entry)) {
    if (entry->ptr == ptr) {
      return entry;
    }
  }

  return 0;
}

void mc_free(void* ptr) {
  if (!mc_bucket_can_belong(ptr)) {
    mc_huge_t* entry = mc__get_huge(ptr);
    assert(entry && "Trying to mc_free memory allocated not with mc_alloc.");
    osfree(ptr, entry->size);

    mc_bucket_free(entry);
    return;
  }

  mc_bucket_t* bucket = mc_bucket_for(ptr);
  mc_bucket_free(ptr);

  if (mc_bucket_empty(bucket)) {
    mc_header_t* header = mc_bucket_header(bucket);
    if (header->prev) {
      header->prev->next = header->next;
    } else {
      mc_start = header->next;
    }

    if (header->next) {
      header->next->prev = header->prev;
    }

    osfree(bucket, mc_bucket_SIZE);
  }
}

void* mc_realloc(void* ptr, size_t newsize) {
  size_t size;
  if (mc_bucket_can_belong(ptr)) {
    if (mc_bucket_extend(ptr, newsize)) {
      return ptr;
    }

    size = mc_bucket_size(ptr);
  } else {
    mc_huge_t* entry = mc__get_huge(ptr);
    assert(entry && "Trying to mc_realloc memory allocated not with mc_alloc.");

    size = entry->size;
  }

  void* newptr = mc_alloc(newsize);
  if (!newptr) {
    return 0;
  }

  memcpy(newptr, ptr, newsize < size ? newsize : size);
  mc_free(ptr);

  return newptr;
}

int main() {
  mc_init();

  char* str = mc_alloc(8);
  strcpy(str, "0123456");

  str = mc_realloc(str, 16);
  assert(str && strcmp(str, "0123456") == 0);

  strcpy(str, "0123456789abcde");

  str = mc_realloc(str, 32);
  assert(str && strcmp(str, "0123456789abcde") == 0);

  mc_free(str);

  void* ptrs[128];
  for (int i = 0; i < 128; i++) {
    ptrs[i] = mc_alloc(512);
  }

  for (int i = 0; i < 128; i++) {
    mc_free(ptrs[i]);
  }

  void* mem = mc_alloc(65536u);
  assert(mem);
  mem = mc_realloc(mem, 2*65536u);
  assert(mem);

  mc_free(mem);
}
