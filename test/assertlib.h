#ifndef __FAL_ASSERTLIB_H__
#define __FAL_ASSERTLIB_H__


#include <fal/utils.h>
#include <stdio.h>
#include <stdlib.h>

#define fal_asserteq(Actual, Expected, Type, Spec) do {          \
    if ((Actual) != (Expected)) {                                \
      fprintf(stderr, __FILE__ ":" FAL_STR(__LINE__) " in %s: "  \
        "Expected " Spec " but got " Spec " (" #Actual ")\n",    \
        __func__, (Type)(Expected), (Type)(Actual));             \
      exit(1);                                                   \
    }                                                            \
  } while(0)

#endif /* __FAL_ASSERTLIB_H__ */
