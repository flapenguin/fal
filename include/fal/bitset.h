/* Copyright (c) 2016 Andrey Roenko
 * This file is part of fal project which is released under MIT license.
 * See file LICENSE or go to https://opensource.org/licenses/MIT for full
 * license details.
*/
#ifndef __FAL_BITSET_H__
#define __FAL_BITSET_H__

#include <limits.h>

#define fal_bitset__mask(Ix) (1 << ((Ix) % CHAR_BIT))
#define fal_bitset__slot(BitSet, Ix) ((char*)BitSet)[(Ix) / CHAR_BIT]

#define fal_bitset_size(Len) \
  (((Len) + CHAR_BIT - 1) / CHAR_BIT)
#define fal_bitset_test(BitSet, Ix) \
  (!!( fal_bitset__slot(BitSet, (Ix)) & fal_bitset__mask(Ix) ))
#define fal_bitset_set(BitSet, Ix) \
  (fal_bitset__slot(BitSet, (Ix)) |= fal_bitset__mask(Ix))
#define fal_bitset_clear(BitSet, Ix) \
  (fal_bitset__slot(BitSet, (Ix)) &= ~fal_bitset__mask(Ix))

#define fal_bitset_assign(BitSet, Ix, Value) \
  ((Value) ? fal_bitset_set(BitSet, (Ix)) : fal_bitset_clear(BitSet, (Ix)))

#endif /* __FAL_BITSET_H__ */
