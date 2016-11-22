/* Copyright (c) 2016 Andrey Roenko
 * This file is part of fal project which is released under MIT license.
 * See file LICENSE or go to https://opensource.org/licenses/MIT for full
 * license details.
*/
#ifndef __FAL_UTILS_H__
#define __FAL_UTILS_H__

#define FAL__CONCAT(A, B) A ## B
#define FAL_CONCAT(A, B) FAL__CONCAT(A, B)

#define FAL__STR(X) #X
#define FAL_STR(X) FAL__STR(X)

#define FAL_UNUSED(X) ((void)X)
#define FAL_ARRLEN(Arr) (sizeof(Arr)/sizeof(Arr[0]))

#define FAL_STATIC_ASSERT(Condition) \
    ((void) sizeof(char[1 - 2*!(Condition)]) )

#endif /* __FAL_UTILS_H__ */
