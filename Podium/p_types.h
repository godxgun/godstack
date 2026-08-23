#ifndef _PODIUM_TYPES_
#define _PODIUM_TYPES_

#define NANOS_PER_SEC (1000*1000*1000)
#define P_PI_HALF  1.57079632679f
#define P_PI       3.14159265f
#define P_PI2      6.28318531f
#define P_PI_POW2  9.86960440f

#ifdef NDEBUG
    #define assert(x) ((void)0)
#else
    #ifndef assert
        #define assert(x) \
            do { \
                if (!(x)) { \
                    __builtin_trap(); \
                } \
            } while (0)
    #endif
#endif

#ifndef BUFSIZ
    #define BUFSIZ 8192
#endif

typedef unsigned char      u8;
typedef signed char        i8;
typedef unsigned short     u16;
typedef short              i16;
typedef unsigned int       u32;
typedef int                i32;
typedef unsigned long long u64;
typedef long long          i64;
typedef float              f32;
typedef double             f64;

#ifndef __cplusplus
#define bool _Bool
#define false 0
#define true 1
#endif

/* C++ uses static_assert, C11 uses _Static_assert */
#ifdef __cplusplus
    #define STATIC_ASSERT static_assert
#else
    #define STATIC_ASSERT _Static_assert
#endif

STATIC_ASSERT(sizeof(u8)  == 1, "u8 must be 1 byte");
STATIC_ASSERT(sizeof(u16) == 2, "u16 must be 2 bytes");
STATIC_ASSERT(sizeof(u32) == 4, "u32 must be 4 bytes");
STATIC_ASSERT(sizeof(u64) == 8, "u64 must be 8 bytes");

STATIC_ASSERT(sizeof(i8)  == 1, "i8 must be 1 byte");
STATIC_ASSERT(sizeof(i16) == 2, "i16 must be 2 bytes");
STATIC_ASSERT(sizeof(i32) == 4, "i32 must be 4 bytes");
STATIC_ASSERT(sizeof(i64) == 8, "i64 must be 8 bytes");

STATIC_ASSERT(sizeof(f32) == 4, "f32 must be 4 bytes");
STATIC_ASSERT(sizeof(f64) == 8, "f64 must be 8 bytes");
#define U8_MAX  255
#define U16_MAX 65535
#define U32_MAX 4294967295

/* Architecture specific sizing */
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
    typedef u64 usize;
    typedef u64 uintptr;
    typedef u32 idx;
    STATIC_ASSERT(sizeof(usize) == 8, "usize must be 8 bytes on 64-bit");
#else
    typedef u32 usize;
    typedef u32 uintptr;
    typedef u16 idx;
    STATIC_ASSERT(sizeof(usize) == 4, "usize must be 4 bytes on 32-bit");
#endif

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void*)0)
    #endif
#endif

#endif // _PODIUM_TYPES_
