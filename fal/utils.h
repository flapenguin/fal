#ifndef __FAL_UTILS_H__
#define __FAL_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define FAL_STATIC_ASSERT(Condition) \
    ((void) sizeof(char[1 - 2*!(Condition)]) )

#ifdef __cplusplus
}
#endif

#endif /* __FAL_UTILS_H__ */