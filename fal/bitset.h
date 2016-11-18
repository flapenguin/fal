#ifndef __FAL_BITSET_H__
#define __FAL_BITSET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

#define fal_bitset__mask(Ix) (1 << ((Ix) % CHAR_BIT))
#define fal_bitset__slot(BitSet, Ix) ((char*)BitSet)[(Ix) / CHAR_BIT]

#define fal_bitset_test(BitSet, Ix) (fal_bitset__slot(BitSet, (Ix)) & fal_bitset__mask(Ix))
#define fal_bitset_set(BitSet, Ix) (fal_bitset__slot(BitSet, (Ix)) |= fal_bitset__mask(Ix))
#define fal_bitset_clear(BitSet, Ix) (fal_bitset__slot(BitSet, (Ix)) &= ~fal_bitset__mask(Ix))

#define fal_bitset_assign(BitSet, Ix, Value) \
  ((Value) ? fal_bitset_set(BitSet, (Ix)) : fal_bitset_clear(BitSet, (Ix)))

#ifdef __cplusplus
}
#endif

#endif /* __FAL_BITSET_H__ */