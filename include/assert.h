#ifndef ASSERT_H
#define ASSERT_H

//#define NDEBUG
#include <panic.h>
#ifdef NDEBUG

#define ASSERT(expr) ((void)0)

#else

#define ASSERT(expr)                            \
    do {                                        \
        if (!(expr)) {                          \
            panic(#expr, __FILE__, __LINE__);   \
        }                                       \
    } while (0)

#endif

#endif
