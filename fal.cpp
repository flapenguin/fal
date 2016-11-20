// fal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <cstddef>
#include <cstdint>
#include <assert.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

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

    strncpy((char*)fal_arena_user_lo(arena), "qwertyasdfghzxcvbn", fal_arena_USER_LO_BYTES);
    strncpy((char*)fal_arena_user_hi(arena), "1234567890", fal_arena_USER_HI_BYTES);

    auto print_arena = [&]() {
        cout << (fal_arena_bumptop(arena) - fal_arena_FIRST) << "/" << fal_arena_TOTAL << " blocks"
            << "(starting at " << fal_arena_FIRST << ") "
            << "lo=" << std::string((const char*)fal_arena_user_lo(arena), fal_arena_USER_LO_BYTES)
            << " "
            << "hi=" << std::string((const char*)fal_arena_user_hi(arena), fal_arena_USER_HI_BYTES)
            << ":\n\t";

        for (void* ptr = fal_arena_first(arena); ptr; ptr = fal_arena_next_noskip(ptr)) {
            bool used = fal_arena_used(ptr);
            bool marked = fal_arena_marked(ptr);
            cout << (!used ? '.' : (marked ? 'x' : 'o'));

            for (int i = 0; i < fal_arena_size(ptr) - 1; i++) {
                cout << (used ? '-' : '.');
            }
        }

        cout << endl;
    };

    print_arena();

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
        print_arena();
    }

    std::vector<int> to_mark{ {1, 1, 3, 5, 7} };
    for (int mark : to_mark) {
        if (mark < 0) {
            fal_arena_unmark(allocations[-mark - 1]);
        } else {
            fal_arena_mark(allocations[mark - 1]);
        }
        print_arena();
    }

    fal_arena_mark_all(arena, 0);
    print_arena();

    fal_arena_mark_all(arena, 1);
    print_arena();

    return 0;
}
