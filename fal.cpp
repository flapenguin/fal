// fal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstddef>
#include <cstdint>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <vector>

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
        void* bitset_b = fal_arena__bitset_b(arena);

        cout << (*top - fal_arena__FIRST) << "/" << fal_arena_TOTAL << " blocks"
            << "(starting at " << fal_arena__FIRST << "):\n\t";

        for (size_t ix = fal_arena__FIRST; ix < std::min((uint32_t)fal_arena__BLOCKS, (uint32_t)*top); ix++) {
            bool allocation_start = fal_bitset_test(bitset_b, ix);
            bool marked = fal_arena_marked(fal_arena__block(arena, ix));

            cout << (allocation_start ? (marked ? 'M' : 'x') : (marked ? '-' : '.'));
        }

        cout << endl;
    };

    auto p = print_bitset;

    std::vector<int> sizes{ { 1, 15, 16, 17, 512, -2, 54, 76 } };
    std::vector<void*> allocations;

    for (int size : sizes) {
        if (size < 0) {
            auto it = allocations.end() + size;
            fal_arena_free(*it);
        } else {
            void* x = fal_arena_alloc(arena, size);
            allocations.push_back(x);
        }
        print_bitset();
    }

    std::vector<int> to_mark{ {1, 1, 3, 5, 7} };
    for (int mark : to_mark) {
        if (mark < 0) {
            fal_arena_unmark(allocations[-mark - 1]);
        } else {
            fal_arena_mark(allocations[mark - 1]);
        }
        print_bitset();
    }

    return 0;
}
