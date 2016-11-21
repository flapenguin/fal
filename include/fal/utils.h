#ifndef __FAL_UTILS_H__
#define __FAL_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define FAL__CONCAT(A, B) A ## B
#define FAL_CONCAT(A, B) FAL__CONCAT(A, B)

#define FAL__STR(X) #X
#define FAL_STR(X) FAL__STR(X)

#define FAL_UNUSED(X) ((void)X)
#define FAL_ARRLEN(Arr) (sizeof(Arr)/sizeof(Arr[0]))

#define FAL_STATIC_ASSERT(Condition) \
    ((void) sizeof(char[1 - 2*!(Condition)]) )

#ifdef __cplusplus
}
#endif

#endif /* __FAL_UTILS_H__ */
