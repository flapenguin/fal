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

        cout << (*top - fal_arena__FIRST) << "/" << fal_arena_TOTAL << " blocks"
            << "(starting at " << fal_arena__FIRST << "):\n\t";

        for (size_t ix = fal_arena__FIRST; ix < std::min((uint32_t)fal_arena__BLOCKS, (uint32_t)*top); ix++) {
            bool a = fal_bitset_test(bitset_a, ix);
            bool b = fal_bitset_test(bitset_b, ix);

            cout << (a ? (b ? 'M' : '-') : (b ? 'x' : '.'));
        }

        cout << endl;
    };

    auto p = print_bitset;

    p();
    fal_arena_alloc(arena, 1);
    p();
    fal_arena_alloc(arena, 15);
    p();
    fal_arena_alloc(arena, 16);
    p();
    void* two_blocks = fal_arena_alloc(arena, 17);
    p();
    void* huge = fal_arena_alloc(arena, 512);
    p();
    fal_arena_free(two_blocks);
    p();
    fal_arena_alloc(arena, 32);
    p();
    fal_arena_free(huge);
    p();

    return 0;
}
