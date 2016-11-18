// fal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstddef>
#include <cstdint>
#include <assert.h>
#include <iostream>
#include <algorithm>

#define FAL_ARENA_BLOCK_POW 2u  /* 32 bytes*/
#define FAL_ARENA_POW       16u /* 64 KiB */
#include "fal/arena.h"

#define __VC_EXTRALEAN
#include <Windows.h>
#undef min
#undef max


int main(int argc, char* argv[]) {
    using std::cout;
    using std::endl;

    void* mem = VirtualAlloc(0, FAL_ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(mem && "VirtualAlloc");

    fal_arena_t* arena = (fal_arena_t*)mem;
    fal_arena_init(arena);

    uint16_t* top = fal_arena__top_ptr(arena);
    cout << "top@" << top << " = " << *top << endl;

    auto print_bitset = [&]() {
        void* bitset_a = fal_arena__bitset_a(arena);
        void* bitset_b = fal_arena__bitset_b(arena);

        cout << (*top - FAL_ARENA_FIRST) << "/" << FAL_ARENA_TOTAL << " blocks"
            << "(starting at " << FAL_ARENA_FIRST << ") : ";

        for (size_t ix = FAL_ARENA_FIRST; ix < std::min(FAL_ARENA_BLOCKS, (uint32_t)*top); ix++) {
            bool a = fal_bitset_test(bitset_a, ix);
            bool b = fal_bitset_test(bitset_b, ix);

            cout << (a ? (b ? 'M' : '-') : (b ? 'x' : '?'));
        }

        cout << endl;
    };

    print_bitset();

    fal_arena_alloc(arena, 1);
    cout << *top << endl;
    fal_arena_alloc(arena, 15);
    cout << *top << endl;
    fal_arena_alloc(arena, 16);
    cout << *top << endl;
    fal_arena_alloc(arena, 17);
    cout << *top << endl;
    fal_arena_alloc(arena, 512);
    cout << *top << endl;

    print_bitset();

    return 0;
}
