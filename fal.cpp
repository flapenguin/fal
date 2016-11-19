// fal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstddef>
#include <cstdint>
#include <assert.h>
#include <iostream>
#include <algorithm>

#define FAL_ARENA_DEF_BLOCK_POW 4u  /* 16 bytes*/
#define FAL_ARENA_DEF_POW       16u /* 64 KiB */
#define FAL_ARENA_DEF_NAME      fal_arena
#include "fal/arena.h"

#define __VC_EXTRALEAN
#include <Windows.h>
#undef min
#undef max


int main(int argc, char* argv[]) {
    using std::cout;
    using std::endl;

    void* mem = VirtualAlloc(0, fal_arena_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(mem && "VirtualAlloc");

    fal_arena_t* arena = (fal_arena_t*)mem;
    fal_arena_init(arena);

    uint16_t* top = fal_arena__top_ptr(arena);
    cout << "top@" << top << " = " << *top << endl;

    auto print_bitset = [&]() {
        void* bitset_a = fal_arena__bitset_a(arena);
        void* bitset_b = fal_arena__bitset_b(arena);

        cout << (*top - fal_arena_FIRST) << "/" << fal_arena_TOTAL << " blocks"
            << "(starting at " << fal_arena_FIRST << "):\n\t";

        for (size_t ix = fal_arena_FIRST; ix < std::min((uint32_t)fal_arena_BLOCKS, (uint32_t)*top); ix++) {
            bool a = fal_bitset_test(bitset_a, ix);
            bool b = fal_bitset_test(bitset_b, ix);

            cout << (a ? (b ? 'M' : '-') : (b ? 'x' : '.'));
        }

        cout << endl;
    };

    print_bitset();

    fal_arena_bumpalloc(arena, 1);
    cout << *top << endl;
    fal_arena_bumpalloc(arena, 15);
    cout << *top << endl;
    fal_arena_bumpalloc(arena, 16);
    cout << *top << endl;
    void* x = fal_arena_bumpalloc(arena, 17);
    cout << *top << endl;
    void* last = fal_arena_alloc(arena, 512);
    cout << *top << endl;

    fal_arena_free(x);
    fal_arena_alloc(arena, 32);

    print_bitset();

    fal_arena_free(last);

    print_bitset();

    return 0;
}
