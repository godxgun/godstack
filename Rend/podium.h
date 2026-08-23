/* ===========================================================================   
 * PODIUM - The Finished Platform Layer - Copyright (c) 2026 Vasco Alves
 *
 * ---------------------------------------------------------------------------   
 * To stand on the shoulders of giants...
 * Giants must stand still!"
 * - Eskil Steenberg - "You should finish your software" – BSC 2025
 * ---------------------------------------------------------------------------   
 *
 * PREFIX: P_ (types) or p_ (functions & variables)
 *
 * GOAL:
 * Replace the standard library, always talk to the os directly. 
 * Besides some optimization and security benefits. Obviously some standard 
 * library functions are highly optimized, namely memory and math
 * into our code and debug it. Still a lot is left to be desired
 * and so this library includes it via "modules" which can be imported
 * by flags.
 * 
 * DESIGN:
 * The API is finished. It's done. Newer modules may be added in the future
 * but that should not change the underlying core. New platforms may also
 * be added, but the API stays the same.
 *
 * COMPILE TIME FLAGS
 * | Name            | Description                                             |
 * |-----------------|---------------------------------------------------------|
 * | PODIUM_WIN32    | Used to explicitly compile for Windows.                 |
 * | PODIUM_LINUX    | Used to explicitly compile for Linux and FreeBSD.       |
 *
 * MODULES
 * | Name            | Description                                             |
 * |-----------------|---------------------------------------------------------|
 * | P_MODULE_VULKAN | Gives access to vulkan related utilities.               |
 * | P_MODULE_STRING | Include string manipulation utilities.                  |
 * | P_MODULE_MATH   | Trigonometry and linear algebra functions.              |
 * | P_MODULE_ALLOC  | Memory allocators: Arena, Stack & Pool.                 |
 *
 * =========================================================================== */

#ifndef _PODIUM_H_
#define _PODIUM_H_

#define PODIUM_MAJOR "0" // needs more revisions and use to reach 1.0 status
#define PODIUM_MINOR "2" // vulkan module
#define PODIUM_PATCH "2" // gamepad

#define POAPI static inline
#define PODEF static inline

/*
 * Auto-detect platforms.
 */ 

/* Windows */
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define PODIUM_WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
/* Apple */
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #define PODIUM_APPLE
    #if TARGET_OS_IPHONE
        #define PODIUM_IOS
    #else
        #define PODIUM_MACOS
    #endif
/* Linux / BSD */ 
#elif defined(__linux__) || defined(__FreeBSD__)
    #define PODIUM_LINUX 
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200112L
    #endif 
#endif 

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h> /* memcpy */
#include <stdio.h> /* vsnprintf */

/*
 * NOTE: The <REPLACE> comments are used by the build
 * script to know what includes to inline into the
 * final single header file! 
 */

/* --- Start of p_types.h --- */
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
/* --- End of p_types.h --- */
/* --- Start of p_log.h --- */
/*
 * Logging!
 */

#include <stdlib.h>

#ifdef DEBUG
#define LOG_DEBUG(...) PRINTF(__VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

/* fatal and error always enabled */
#define PFATAL(message, ...) p_log_printf(P_LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#define PERROR(message, ...) p_log_printf(P_LOG_LEVEL_ERROR, message, ##__VA_ARGS__)

/* warn and info enabled by default but can be disabled */
#define P_LOG_WARN_ENABLED 1
#define P_LOG_INFO_ENABLED 1

#if P_LOG_WARN_ENABLED == 1
#define PWARN(message, ...) p_log_printf(P_LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
#define PWARN(message, ...)
#endif

#if P_LOG_INFO_ENABLED == 1
#define PINFO(message, ...) p_log_printf(P_LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else 
#define PINFO(message, ...)
#endif

#if P_LOG_DEBUG_ENABLED == 1
#define PDEBUG(message, ...) p_log_printf(P_LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else 
#define PDEBUG(message, ...)
#endif

#if P_LOG_TRACE_ENABLED == 1
#define PTRACE(message, ...) p_log_printf(P_LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else 
#define PTRACE(message, ...)
#endif

typedef enum P_Log_Level {
    P_LOG_LEVEL_FATAL = 0,
    P_LOG_LEVEL_ERROR,
    P_LOG_LEVEL_WARN,
    P_LOG_LEVEL_INFO,
    P_LOG_LEVEL_DEBUG,
    P_LOG_LEVEL_TRACE,
    P_COUNT_LOG_LEVEL
} P_Log_Level;

#define P_PREFIX_LEN 7
static const char *p_prefix[P_COUNT_LOG_LEVEL] = {
    [P_LOG_LEVEL_FATAL] = "[FATAL]",
    [P_LOG_LEVEL_ERROR] = "[ERROR]",
    [P_LOG_LEVEL_WARN]  = "[WARNI]",
    [P_LOG_LEVEL_INFO]  = "[INFOR]",
    [P_LOG_LEVEL_DEBUG] = "[DEBUG]",
    [P_LOG_LEVEL_TRACE] = "[TRACE]",
}; 

#define P_MAX_ALLOCS 512

typedef struct P_DebugMemoryInfo {
    void *ptr;
    size_t size;
    const char *file;
    const char *func;
    int line;
} P_DebugMemoryInfo;

static uint64_t p_alloc_count = 0;
static P_DebugMemoryInfo p_ptr_array[P_MAX_ALLOCS];

static inline void *
p_debug_malloc_impl(size_t size, const char *file, int line, const char *func)
{
    void *ptr = malloc(size);
    printf("[ALLOC] %p (%zu bytes) -> %s:%d %s()\n", ptr, size, file, line, func);

    if (ptr) {
        if (p_alloc_count < P_MAX_ALLOCS) {
            p_ptr_array[p_alloc_count++] = (P_DebugMemoryInfo){
                .ptr = ptr,
                .size = size,
                .file = file,
                .func = func,
                .line = line
            };
        } else {
            fprintf(stderr, "[ERROR] Debug allocator tracking capacity (%d) exceeded!\n", P_MAX_ALLOCS);
        }
    }
    return ptr;
}

static inline void
p_debug_free_impl(void *ptr, const char *file, int line, const char *func)
{
    printf("[FREE]  %p -> %s:%d %s()\n", ptr, file, line, func);

    if (!ptr) return; // Standard C allows free(NULL)

    bool found = false;
    uint64_t index = 0;

    /* Find allocation metadata */
    for (index = 0; index < p_alloc_count; ++index) {
        if (p_ptr_array[index].ptr == ptr) {
            found = true;
            break;
        }
    }

    if (found) {
        /* Shift array left to remove tracked struct */
        for (uint64_t j = index; j < p_alloc_count - 1; ++j) {
            p_ptr_array[j] = p_ptr_array[j + 1];
        }
        p_alloc_count--;
    } else {
        fprintf(stderr, "[WARNING] Attempted to free untracked/double-freed pointer %p at %s:%d %s()\n", 
                ptr, file, line, func);
    }

    free(ptr);
}

static inline void *
p_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func)
{
    if (!ptr) {
        return p_debug_malloc_impl(size, file, line, func);
    }
    if (size == 0) {
        p_debug_free_impl(ptr, file, line, func);
        return NULL;
    }

    void *new_ptr = realloc(ptr, size);
    printf("[REALLOC] %p -> %p (%zu bytes) -> %s:%d %s()\n", ptr, new_ptr, size, file, line, func);

    if (new_ptr) {
        bool found = false;
        for (uint64_t i = 0; i < p_alloc_count; ++i) {
            if (p_ptr_array[i].ptr == ptr) {
                p_ptr_array[i].ptr = new_ptr;
                p_ptr_array[i].size = size;
                p_ptr_array[i].file = file;
                p_ptr_array[i].line = line;
                p_ptr_array[i].func = func;
                found = true;
                break;
            }
        }
        if (!found) {
            if (p_alloc_count < P_MAX_ALLOCS) {
                p_ptr_array[p_alloc_count++] = (P_DebugMemoryInfo){
                    .ptr = new_ptr,
                    .size = size,
                    .file = file,
                    .func = func,
                    .line = line
                };
            }
        }
    }
    return new_ptr;
}

static void
p_debug_memory_report(void) 
{
    printf("\n==================== MEMORY REPORT ====================\n");
    printf("Remaining unfreed allocations: %lu\n", (unsigned long)p_alloc_count);
    
    for (uint64_t i = 0; i < p_alloc_count; ++i) {
        P_DebugMemoryInfo *info = &p_ptr_array[i];
        printf("[LEAK] %p (%zu bytes) allocated at %s:%d in %s()\n", 
               info->ptr, info->size, info->file, info->line, info->func);
    }
    printf("=======================================================\n");
}
/* --- End of p_log.h --- */
/* --- Start of p_ds.h --- */
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#define ARRAY_GROW_FACTOR 2

typedef struct {
    u32 size;
    u32 capacity;
} DynamicArrayHeader;

#define DARRAY_INIT_CAPACITY 64
#define p_darray_push(array, var)                                              \
    do {                                                                         \
        if (array == NULL) {                                                       \
            DynamicArrayHeader *header =                                             \
            malloc(DARRAY_INIT_CAPACITY * sizeof *array + sizeof *header);     \
            header->size = 0;                                                        \
            header->capacity = DARRAY_INIT_CAPACITY;                                 \
            array = (void *)(header + 1);                                            \
        }                                                                          \
        DynamicArrayHeader *header = (DynamicArrayHeader *)(array) - 1;            \
        if (header->size >= header->capacity - 1) {                                \
            unsigned long new_capacity = header->capacity * 2;                       \
            DynamicArrayHeader *new_header =                                         \
            realloc(header, new_capacity * sizeof *array + sizeof *header);    \
            if (!new_header)                                                         \
            break;                                                                 \
            header = new_header;                                                     \
            array = (void *)(header + 1);                                            \
            header->capacity = new_capacity;                                         \
        }                                                                          \
        (array)[header->size++] = var;                                             \
    } while (0)

#define p_darray_len(array) ((array) ? ((DynamicArrayHeader *)(array) - 1)->size : 0)
#define p_darray_destroy(array)                                                \
{                                                                            \
    free((DynamicArrayHeader *)(array) - 1);                                 \
}

#define p_darray_shrink(array) \
    do { \
        DynamicArrayHeader *header = (array) ? ((DynamicArrayHeader *)(array) - 1) : 0;\
        if (header) {\
            if (header->capacity > 4) {\
                unsigned long new_capacity = (header->capacity >> 1) + (header->capacity & 1); \
                void *new_header = realloc(header, new_capacity * sizeof *array + sizeof *header); \
                if (!new_header) break; \
                header = new_header; \
                array = (void *)(header + 1); \
                header->capacity = new_capacity; \
            }\
        }\
    } while (0)
/* --- End of p_ds.h --- */

/*
 * Main Platform Types (Keyboard, Events, Context, etc.)
 */

typedef enum {
    P_KEYMOD_ALT = 0,
    P_KEYMOD_SHIFT,
    P_KEYMOD_CTRL,
    P_KEYMOD_CAPS,
} P_KeyMod;

typedef enum {
    P_KEY_UNKNOWN = 0,
    P_KEY_UP, P_KEY_DOWN, P_KEY_LEFT, P_KEY_RIGHT,
    P_KEY_SPACE, P_KEY_ESCAPE, P_KEY_ENTER,
    P_KEY_A, P_KEY_B, P_KEY_C, P_KEY_D, P_KEY_E, P_KEY_F, P_KEY_G, P_KEY_H, P_KEY_I,
    P_KEY_J, P_KEY_K, P_KEY_L, P_KEY_M, P_KEY_N, P_KEY_O, P_KEY_P, P_KEY_Q, P_KEY_R,
    P_KEY_S, P_KEY_T, P_KEY_U, P_KEY_V, P_KEY_W, P_KEY_X, P_KEY_Y, P_KEY_Z,
} P_KeyCode;

/* Platform event type */
typedef enum {
    P_EVENT_NONE = 0,
    P_EVENT_KEY_DOWN,
    P_EVENT_KEY_UP,
    P_EVENT_WINDOW_CLOSE,
    P_EVENT_WINDOW_RESIZE,
    P_EVENT_POINTER,
    P_EVENT_CONTROLLER_CONNECTED,
    P_EVENT_CONTROLLER_DISCONNECTED,
} P_EvenType;

typedef enum {
    P_POINTER_MOVED = 0,
    P_POINTER_PRESSED,
    P_POINTER_RELEASED
} P_PointerState;

typedef struct {
    P_EvenType type;
    union {
        struct {P_KeyCode key; P_KeyMod mod; } key;
        struct { u32 width, height; } resize;
        struct { P_PointerState state; u32 button; u32 x, y; } pointer;
    };
} P_Event;

/*
 * Platform dependent functions to be implemented by each layer.
 */

POAPI u64  p_get_time(void); // Get time in nanoseconds.
POAPI void p_sleep_ns(i64 ns); // Sleep for nanoseconds.

POAPI void p_stdout(void *msg, usize bytes); // Print buffer to stdout.
PODEF void p_log_printf(P_Log_Level level, const char* src, ...); // Print buffer to stdout.


POAPI void* p_file_alloc(const char *path, unsigned long *buf_size);
POAPI bool  p_file_exists(const char *path); // Check if file exists.
POAPI usize p_file_size(const char *path); // Check file size.
POAPI void  p_file_load(const char *path, void *buf_ptr, unsigned long buf_size); // Load file to buffer.
PODEF void  p_file_write(const char *path, void *buf_ptr, unsigned long buf_size); // Write buffer to file.

typedef struct  P_Window_Impl   P_Window; 
POAPI bool p_window_open(P_Window *win, int width, int height, const char *title); // Opens a window. Returns false on failure.
POAPI void p_window_close(P_Window *win); // Close window.
POAPI bool p_window_is_open(P_Window *win); // Check if window is open.
POAPI void p_window_size(P_Window *win, int *window_width, int *window_height); // Get window size.
POAPI void p_window_draw(P_Window *win, u32 *pixels, int width, int height); // Draw directly to the window's buffer. Normally much slower than using the GPU.
POAPI bool p_window_poll_event(P_Window *win, P_Event *ev); // Returns true while there are events to poll. Write event data to a pointer.

POAPI void p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc);
POAPI void p_audio_quit();
POAPI void p_audio_write(const i16 *samples, usize count);

/*
 * The Vulkan Module is platform specific
 */

#ifdef P_MODULE_VULKAN
#include <vulkan/vulkan.h>
POAPI char** p_vulkan_get_extensions();
POAPI bool   p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface);
#endif

/*
 * The Remaining Modules are PLATFORM INDEPENDENT!
 */

#ifdef P_MODULE_STRING
/* --- Start of p_string.h --- */
typedef struct {
    const char *cstr;
    u32 len;
} P_StringView;

typedef struct {
    P_StringView sv;
} P_StringBuilder;

/* Classic C string functions */
PODEF char* p_find_char(char *buf, char needle);
PODEF bool  p_strcmp(const char *a, const char *b);
PODEF int   p_strlen(const char *a);
PODEF int   p_strtoi(const char *str);
PODEF char* p_strtok(const char *str, char c);

/* String views */
PODEF P_StringView  p_strview(const char *cstr);
PODEF void          p_strview_chop_left(P_StringView *sv, u32 n);
PODEF void          p_strview_chop_right(P_StringView *sv, u32 n);
PODEF P_StringView  p_strview_chop_delim(P_StringView *sv, char delim);
PODEF P_StringView  p_strview_chop_type(P_StringView *sv, int(*istype)(int));
PODEF void          p_strview_trim_left(P_StringView *sv);
PODEF void          p_strview_trim_right(P_StringView *sv);
PODEF void          p_strview_trim(P_StringView *sv);

/* Classification */
PODEF int p_is_space(int c);
PODEF int p_is_digit(int c);
PODEF int p_is_alpha(int c);
PODEF int p_is_alnum(int c);
PODEF int p_is_upper(int c);
PODEF int p_is_lower(int c);
PODEF int p_is_hex(int c);

/* Transformation */
PODEF int p_to_lower(int c);
PODEF int p_to_upper(int c);
/* --- End of p_string.h --- */
#endif

#ifdef P_MODULE_MATH
/* --- Start of p_math.h --- */
typedef struct {int x, y; } Vec2i;
typedef struct {float x, y; } Vec2f;
typedef struct {float x, y, z; } Vec3f;
typedef struct { float m[16];} P_Mat4; // column-major: m[col * 4 + row]
PODEF int   p_ceil(float x);
PODEF float p_cosf(float x);
PODEF float p_sinf(float x);

PODEF Vec2f p_vec2f(float, float);
PODEF Vec2f p_vec2f_add(Vec2f, Vec2f);
PODEF float p_vec2f_cross(Vec2f, Vec2f);
PODEF float p_vec2f_dot(Vec2f, Vec2f);
PODEF Vec2f p_vec2f_mult(Vec2f a, float b);
PODEF Vec2f p_vec2f_sub(Vec2f, Vec2f);
PODEF Vec2i p_vec2i(float, float);
PODEF Vec2i p_vec2i_add(Vec2i, Vec2i);
PODEF float p_vec2i_cross(Vec2i, Vec2i);
PODEF float p_vec2i_dot(Vec2i, Vec2i);
PODEF Vec2i p_vec2i_mult(Vec2i a, float b);
PODEF Vec2i p_vec2i_sub(Vec2i, Vec2i);

PODEF Vec3f p_vec3f(float, float, float);

PODEF P_Mat4 p_mat4_identity(void);
PODEF P_Mat4 p_mat4_mul(P_Mat4 a, P_Mat4 b);
PODEF P_Mat4 p_mat4_translate(Vec3f v);
PODEF P_Mat4 p_mat4_translate_by(P_Mat4 m, Vec3f v);
PODEF P_Mat4 p_mat4_rotate_x(float rad);
PODEF P_Mat4 p_mat4_rotate_x_by(P_Mat4 m, float rad);
PODEF P_Mat4 p_mat4_rotate_y(float rad);
PODEF P_Mat4 p_mat4_rotate_y_by(P_Mat4 m, float rad);
PODEF P_Mat4 p_mat4_rotate_z(float rad);
PODEF P_Mat4 p_mat4_perspective(float fov_rad, float aspect, float near_z, float far_z);
/* --- End of p_math.h --- */
#endif

#ifdef P_MODULE_ALLOC
/* --- Start of p_allocators.h --- */
typedef struct Arena {
    unsigned char *buf;
    usize buf_len;
    usize curr_offset;
    usize prev_offset;
} Arena;

PODEF void      p_arena_create(Arena *a, void *backing_buffer, usize backing_buffer_length);
PODEF void*     p_arena_alloc_align(Arena *a, usize s, usize align);
PODEF void*     p_arena_alloc(Arena *a, usize s);
PODEF void*     p_arena_resize_align(Arena *a, void *old_memory, usize old_size, usize new_size, usize align);
PODEF void*     p_arena_resize(Arena *a, void *old_memory, usize old_size, usize new_size);
PODEF void      p_arena_free_all(Arena *a);
PODEF void      p_arena_destroy(Arena* a);

typedef struct { u8 padding; } Stack_Allocation_Header;
typedef struct Stack {
    unsigned char *buf;
    usize buf_len;
    usize offset;
} Stack;

PODEF void      p_stack_create(Stack *s, void *backing_buffer, usize backing_buffer_length);
PODEF usize     calc_padding_with_header(uintptr ptr, uintptr alignment, usize header_size);
PODEF void*     p_stack_alloc(Stack *s, usize len, usize alignment);
PODEF void      p_stack_free(Stack *s, void *ptr);
PODEF void      p_stack_free_all(Stack* s);
PODEF void      p_stack_destroy(Stack* s);

typedef struct Pool_Free_Node Pool_Free_Node;

typedef struct Pool {
    unsigned char *buf;
    usize chunk_size;
    usize pool_size;
    Pool_Free_Node *head;
} Pool;

struct Pool_Free_Node { struct Pool_Free_Node *next; };
PODEF void      p_pool_create(Pool *p, void *buf, usize buf_len, usize chunk_size, usize chunk_align);
PODEF void*     p_pool_alloc(Pool *p);
PODEF void      p_pool_free(Pool *p, void *ptr);
PODEF void      p_pool_free_all(Pool *p);
PODEF void      p_pool_destroy(Pool *p);
/* --- End of p_allocators.h --- */
#endif

#endif /* _PODIUM_H_ */

#ifdef PODIUM_IMPLEMENTATION
#undef PODIUM_IMPLEMENTATION

#if defined(PODIUM_SDL)
/* #include "p_platform_sdl.c"  */
#elif defined(PODIUM_WIN32)
/* --- Start of p_platform_win32.c --- */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h> /* gamepad support */

typedef struct {
    HWAVEOUT wave_out;
    WAVEHDR wave_header;
} P_Win32Ctx;

struct P_Window_Impl {
    HWND hwnd;
    HDC hdc;
    BITMAPINFO bitmap_info;
    int width;
    int height;
    bool closed;
};

P_Win32Ctx p_ctx = {0};

static inline P_KeyCode
_win32_translate_vkey(WPARAM vk)
{
    switch (vk) {
        case VK_LEFT:   return P_KEY_LEFT;
        case VK_RIGHT:  return P_KEY_RIGHT;
        case VK_UP:     return P_KEY_UP;
        case VK_DOWN:   return P_KEY_DOWN;
        case VK_SPACE:  return P_KEY_SPACE;
        case VK_ESCAPE: return P_KEY_ESCAPE;
        case VK_RETURN: return P_KEY_ENTER;
        case 'A': return P_KEY_A;
        case 'B': return P_KEY_B;
        case 'C': return P_KEY_C;
        case 'D': return P_KEY_D;
        case 'E': return P_KEY_E;
        case 'F': return P_KEY_F;
        case 'G': return P_KEY_G;
        case 'H': return P_KEY_H;
        case 'I': return P_KEY_I;
        case 'J': return P_KEY_J;
        case 'K': return P_KEY_K;
        case 'L': return P_KEY_L;
        case 'M': return P_KEY_M;
        case 'N': return P_KEY_N;
        case 'O': return P_KEY_O;
        case 'P': return P_KEY_P;
        case 'Q': return P_KEY_Q;
        case 'R': return P_KEY_R;
        case 'S': return P_KEY_S;
        case 'T': return P_KEY_T;
        case 'U': return P_KEY_U;
        case 'V': return P_KEY_V;
        case 'W': return P_KEY_W;
        case 'X': return P_KEY_X;
        case 'Y': return P_KEY_Y;
        case 'Z': return P_KEY_Z;
        default:  return P_KEY_UNKNOWN;
    }
}

POAPI u64
p_get_time(void) 
{
    LARGE_INTEGER count, freq;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&freq);
    return (u64)((count.QuadPart * 1000000000ULL) / freq.QuadPart);
}

POAPI void
p_sleep_ns(i64 ns) 
{
    /* windows sleep is millisecond-based........ */
    Sleep((DWORD)(ns / 1000000));
}

POAPI void
p_stdout(void *msg, usize bytes)
{
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytes_written;
    WriteFile(stdout_handle, msg, bytes, &bytes_written, NULL);
}

POAPI bool
p_file_exists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

POAPI usize 
p_file_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        return data.nFileSizeLow;
    }
    return 0;
}

PODEF void
p_file_load(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytes_read;
        ReadFile(file, buf_ptr, buf_size, &bytes_read, NULL);
        CloseHandle(file);
    }
}

PODEF void
p_file_write(const char *path, void *buf_ptr, unsigned long buf_size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        WriteFile(file, buf_ptr, buf_size, &bytes_written, NULL);
        CloseHandle(file);
    }
}

static LRESULT CALLBACK 
_win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    P_Window *win = (P_Window*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CLOSE:
            /* Handled in poll_event; mark as closed */
            if (win) win->closed = true;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

POAPI bool
p_window_open(P_Window *win, int width, int height, const char *title)
{
    HINSTANCE instance = GetModuleHandleA(NULL);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = _win32_wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "P_Window_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    win->hwnd = CreateWindowExA(0, wc.lpszClassName, title,
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                               NULL, NULL, instance, NULL);

    if (!win->hwnd) return false;

    SetWindowLongPtrA(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
    win->hdc    = GetDC(win->hwnd);
    win->width  = width;
    win->height = height;
    win->closed = false;

    return true;
}

POAPI void
p_window_close(P_Window *win)
{
    if (!win) return;
    if (win->hdc)  ReleaseDC(win->hwnd, win->hdc);
    if (win->hwnd) DestroyWindow(win->hwnd);
    win->hdc  = NULL;
    win->hwnd = NULL;
}

POAPI bool
p_window_is_open(P_Window *win)
{
    return win && win->hwnd != NULL;
}

POAPI void
p_window_size(P_Window *win, int *window_width, int *window_height)
{
    if (!win) return;
    RECT rect;
    GetClientRect(win->hwnd, &rect);
    *window_width  = rect.right  - rect.left;
    *window_height = rect.bottom - rect.top;
    win->width  = *window_width;
    win->height = *window_height;
}

POAPI void
p_window_draw(P_Window *win, u32 *pixels, int width, int height)
{
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height; /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(win->hdc, 
                  0, 0, win->width, win->height, 
                  0, 0, width, height, 
                  pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

POAPI bool
p_window_poll_event(P_Window *win, P_Event *ev) 
{
    if (win->closed) {
        win->closed = false;
        ev->type = P_EVENT_WINDOW_CLOSE;
        return true;
    }

    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        switch (msg.message) {

            case WM_QUIT:
                ev->type = P_EVENT_WINDOW_CLOSE;
                return true;

            /* Key press */
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                ev->type    = P_EVENT_KEY_DOWN;
                ev->key.key = _win32_translate_vkey(msg.wParam);
                return true;

            /* Key release */
            case WM_KEYUP:
            case WM_SYSKEYUP:
                ev->type    = P_EVENT_KEY_UP;
                ev->key.key = _win32_translate_vkey(msg.wParam);
                return true;

            /* Mouse movement */
            case WM_MOUSEMOVE:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_MOVED;
                ev->pointer.button   = 0;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            /* Mouse buttons */
            case WM_LBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 1; /* 1 = LMB, matching X11 convention */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_LBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 1;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_RBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 3; /* 3 = RMB */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_RBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 3;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_MBUTTONDOWN:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = 2; /* 2 = MMB */
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            case WM_MBUTTONUP:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = 2;
                ev->pointer.x        = (u32)(msg.lParam & 0xFFFF);
                ev->pointer.y        = (u32)((msg.lParam >> 16) & 0xFFFF);
                return true;

            /* Window resize */
            case WM_SIZE:
                ev->type          = P_EVENT_WINDOW_RESIZE;
                ev->resize.width  = (u32)(msg.lParam & 0xFFFF);
                ev->resize.height = (u32)((msg.lParam >> 16) & 0xFFFF);
                win->width  = ev->resize.width;
                win->height = ev->resize.height;
                return true;

            default:
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
                break;
        }
    }

    ev->type = P_EVENT_NONE;
    return false;
}


POAPI void
p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc) 
{
    (void)name; (void)desc; /* WinMM doesn't use a stream name */

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = channels;
    wfx.nSamplesPerSec  = sample_rate;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&p_ctx.wave_out, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        exit(1);
    }
}

POAPI void
p_audio_quit()
{
    if (p_ctx.wave_out) {
        waveOutReset(p_ctx.wave_out);
        waveOutClose(p_ctx.wave_out);
        p_ctx.wave_out = NULL;
    }
}

POAPI void
p_audio_write(const i16 *samples, usize count)
{
    if (!p_ctx.wave_out) return;

    WAVEHDR header = {0};
    header.lpData         = (LPSTR)samples;
    header.dwBufferLength = (DWORD)(count * sizeof(i16));
    
    waveOutPrepareHeader(p_ctx.wave_out, &header, sizeof(WAVEHDR));
    waveOutWrite(p_ctx.wave_out, &header, sizeof(WAVEHDR));
    
    /* wait for playback to finish (synchronous, mirrors pa_simple_drain) */
    while (!(header.dwFlags & WHDR_DONE)) Sleep(1);
    
    waveOutUnprepareHeader(p_ctx.wave_out, &header, sizeof(WAVEHDR));
}

#ifdef P_MODULE_VULKAN

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan_win32.h>

POAPI char**
p_vulkan_get_extensions()
{
    char **darr = NULL;
    p_darray_push(darr, "VK_KHR_surface");
    p_darray_push(darr, "VK_KHR_win32_surface");
    return darr;
}

POAPI bool
p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface)
{
    if (!instance || !window || !window->hwnd || !out_surface) {
        return false;
    }

    VkWin32SurfaceCreateInfoKHR create_info = {0};
    create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.pNext     = NULL;
    create_info.flags     = 0;
    create_info.hwnd      = window->hwnd;
    create_info.hinstance = GetModuleHandleA(NULL);

    VkResult result = vkCreateWin32SurfaceKHR(instance, &create_info, allocator, out_surface);
    return (result == VK_SUCCESS);
}
#endif
/* --- End of p_platform_win32.c --- */
#elif defined(PODIUM_LINUX)
/* --- Start of p_platform_linux.c --- */
/* linux */
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
/* x11 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
/* pulse audio */
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
/* joystick */
#include <linux/joystick.h>

typedef struct {
    pa_simple     *audio_stream;
} P_LinuxCtx;

struct P_Window_Impl {
    Display *display;
    XImage  *image;
    Window window;
    GC gc;
    int screen;
    int width;
    int height;
};

P_LinuxCtx p_ctx = {0};

static inline P_KeyCode
_x11_translate_keycode(XEvent xev) 
{
    static KeySym ks;
    ks = XLookupKeysym(&xev.xkey, 0);
    switch (ks) {
        /* Arrow keys */
        case XK_Left:        return P_KEY_LEFT;
        case XK_Right:       return P_KEY_RIGHT;
        case XK_Up:          return P_KEY_UP;
        case XK_Down:        return P_KEY_DOWN;
        /* Common keys */
        case XK_space:       return P_KEY_SPACE;
        case XK_Escape:      return P_KEY_ESCAPE;
        case XK_Return:      return P_KEY_ENTER;
        case XK_A: case XK_a: return P_KEY_A;
        case XK_B: case XK_b: return P_KEY_B;
        case XK_C: case XK_c: return P_KEY_C;
        case XK_D: case XK_d: return P_KEY_D;
        case XK_E: case XK_e: return P_KEY_E;
        case XK_F: case XK_f: return P_KEY_F;
        case XK_G: case XK_g: return P_KEY_G;
        case XK_H: case XK_h: return P_KEY_H;
        case XK_I: case XK_i: return P_KEY_I;
        case XK_J: case XK_j: return P_KEY_J;
        case XK_K: case XK_k: return P_KEY_K;
        case XK_L: case XK_l: return P_KEY_L;
        case XK_M: case XK_m: return P_KEY_M;
        case XK_N: case XK_n: return P_KEY_N;
        case XK_O: case XK_o: return P_KEY_O;
        case XK_P: case XK_p: return P_KEY_P;
        case XK_Q: case XK_q: return P_KEY_Q;
        case XK_R: case XK_r: return P_KEY_R;
        case XK_S: case XK_s: return P_KEY_S;
        case XK_T: case XK_t: return P_KEY_T;
        case XK_U: case XK_u: return P_KEY_U;
        case XK_V: case XK_v: return P_KEY_V;
        case XK_W: case XK_w: return P_KEY_W;
        case XK_X: case XK_x: return P_KEY_X;
        case XK_Y: case XK_y: return P_KEY_Y;
        case XK_Z: case XK_z: return P_KEY_Z;
        default:              return P_KEY_UNKNOWN;
    }
}

POAPI u64
p_get_time(void) 
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return NANOS_PER_SEC * ts.tv_sec + ts.tv_nsec;
}

POAPI void
p_sleep_ns(i64 ns) 
{
    struct timespec ts;
    ts.tv_sec  = ns / 1000000000L;
    ts.tv_nsec = ns % 1000000000L;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

POAPI void
p_stdout(void *msg, usize bytes)
{
    write(1, msg, bytes);
}

POAPI bool
p_file_exists(const char *path)
{
    return (access(path, F_OK) == 0);
}

POAPI usize
p_file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return 0;
}

PODEF void
p_file_load(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, buf_ptr, (size_t)buf_size);
        close(fd);
    }
}

PODEF void
p_file_write(const char *path, void *buf_ptr, unsigned long buf_size) 
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        write(fd, buf_ptr, (size_t)buf_size);
        close(fd);
    }
}

POAPI bool
p_window_open(P_Window *win, int width, int height, const char *title)
{
    win->width = width;
    win->height = height;

    XInitThreads();

    win->display = XOpenDisplay(NULL);
    if (win->display == NULL) {
        PERROR("Cannot open display!");
        return false;
    }

    int screen = DefaultScreen(win->display);
    win->window = XCreateSimpleWindow(
        win->display,
        RootWindow(win->display, screen),
        0, 0, width, height,
        1,
        BlackPixel(win->display, screen),
        BlackPixel(win->display, screen)
    );

    win->screen = screen;

    XStoreName(win->display, win->window, title);
    XSelectInput(win->display, win->window,
             KeyPressMask | KeyReleaseMask |
             ButtonPressMask | ButtonReleaseMask |
             PointerMotionMask | StructureNotifyMask);
    XMapWindow(win->display, win->window);

    win->gc = DefaultGC(win->display, screen);

    Atom wm_delete = XInternAtom(win->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(win->display, win->window, &wm_delete, 1);

    return true;
}

POAPI void
p_window_close(P_Window *win)
{
    if (!win) return;

    if (win->image) {
        // XDestroyImage(win->image);
        win->image = NULL;
    }

    if (win->display) {
        Display *dpy = win->display;
        Window w = win->window;
        win->display = NULL;
        win->window = 0;

        if (w) {
            XDestroyWindow(dpy, w);
        }
        XSync(dpy, False);
    }
}

POAPI bool
p_window_is_open(P_Window *win)
{
    return win && win->display != NULL;
}

POAPI void
p_window_size(P_Window *win, int *window_width, int *window_height)
{
    if (!win) return;

    XWindowAttributes attr;
    XGetWindowAttributes(win->display, win->window, &attr);
    *window_width  = attr.width;
    *window_height = attr.height;

    win->width = attr.width;
    win->height = attr.height;
}

POAPI void
p_window_draw(P_Window *win, u32 *pixels, int width, int height)
{
    int screen = win->screen;

    win->image = XCreateImage(
            win->display,
            DefaultVisual(win->display, screen),
            DefaultDepth(win->display, screen),
            ZPixmap,
            0,
            (char*)pixels,
            width,
            height,
            32,
            width * 4
            );

    int window_width, window_height;
    p_window_size(win, &window_width, &window_height);

    XPutImage(
            win->display,
            win->window,
            win->gc,
            win->image,
            0, 0,
            0, 0,
            win->width,
            win->height);

    XFlush(win->display);
}

POAPI bool
p_window_poll_event(P_Window *win, P_Event *ev) 
{
    if (XPending(win->display)) {
        static XEvent xev;
        XNextEvent(win->display, &xev);

        switch (xev.type) {

            case KeyPress:
                ev->type    = P_EVENT_KEY_DOWN;
                ev->key.key = _x11_translate_keycode(xev);
                return true;

            case KeyRelease:
                ev->type    = P_EVENT_KEY_UP;
                ev->key.key = _x11_translate_keycode(xev);
                return true;

            case ConfigureNotify:
                ev->type          = P_EVENT_WINDOW_RESIZE;
                ev->resize.width  = xev.xconfigure.width;
                ev->resize.height = xev.xconfigure.height;
                return true;

            case ClientMessage:
                if ((Atom)xev.xclient.data.l[0] ==
                    XInternAtom(win->display, "WM_DELETE_WINDOW", False)) {
                    ev->type = P_EVENT_WINDOW_CLOSE;
                    return true;
                }
                break;

            case MotionNotify:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_MOVED;
                ev->pointer.button   = 0;
                ev->pointer.x        = xev.xmotion.x;
                ev->pointer.y        = xev.xmotion.y;
                return true;

            case ButtonPress:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_PRESSED;
                ev->pointer.button   = xev.xbutton.button; /* 1=LMB, 2=MMB, 3=RMB */
                ev->pointer.x        = xev.xbutton.x;
                ev->pointer.y        = xev.xbutton.y;
                return true;

            case ButtonRelease:
                ev->type             = P_EVENT_POINTER;
                ev->pointer.state    = P_POINTER_RELEASED;
                ev->pointer.button   = xev.xbutton.button;
                ev->pointer.x        = xev.xbutton.x;
                ev->pointer.y        = xev.xbutton.y;
                return true;
        }
    }

    ev->type = P_EVENT_NONE;
    return false;
}

POAPI void
p_audio_init(const int sample_rate, const int channels, const char *name, const char *desc) 
{
    pa_sample_spec ss = {
        .format   = PA_SAMPLE_S16LE,
        .rate     = sample_rate,
        .channels = channels,
    };

    int error;
    p_ctx.audio_stream = pa_simple_new(
        NULL,
        name,
        PA_STREAM_PLAYBACK,
        NULL,
        desc,
        &ss,
        NULL,
        NULL,
        &error
    );

    if (!p_ctx.audio_stream) {
        PERROR("PulseAudio init failed: %s\n", pa_strerror(error));
        exit(1);
    }
}

POAPI void
p_audio_quit() 
{
    if (p_ctx.audio_stream) {
        pa_simple_free(p_ctx.audio_stream);
        p_ctx.audio_stream = NULL;
    }
}

POAPI void
p_audio_write(const i16 *samples, usize count)
{
    if (!p_ctx.audio_stream) return;

    int error;
    if (pa_simple_write(p_ctx.audio_stream, samples, count * sizeof(i16), &error) < 0) {
        PERROR("PulseAudio write failed: %s\n", pa_strerror(error));
    }

    pa_simple_drain(p_ctx.audio_stream, &error);
}

#ifdef P_MODULE_VULKAN

#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include <vulkan/vulkan_xlib.h>

POAPI char**
p_vulkan_get_extensions()
{
    char **darr = NULL;
    p_darray_push(darr, "VK_KHR_surface");
    p_darray_push(darr, "VK_KHR_xcb_surface");
    p_darray_push(darr, "VK_KHR_xlib_surface");
    return darr;
}

POAPI bool
p_vulkan_create_surface(P_Window *window, VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface)
{
    if (!instance || !window || !window->display || !window->window || !out_surface) {
        return false;
    }

    VkXlibSurfaceCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.dpy = window->display;
    create_info.window = window->window;

    VkResult result = vkCreateXlibSurfaceKHR(instance, &create_info, allocator, out_surface);
    
    return (result == VK_SUCCESS);
}
#endif
/* --- End of p_platform_linux.c --- */
#endif /* linux */


POAPI void* p_file_alloc(const char *path, unsigned long *buf_size) {
    if (p_file_exists(path) == false) {
        return NULL;
    }

    usize file_size = p_file_size(path);
    *buf_size = file_size;

    void *ptr = malloc(file_size);
    if (ptr) {
        p_file_load(path, ptr, file_size); // Load file to buffer.
    }
    return ptr;
}


/*
 * Import Module Code
 */

#ifdef P_MODULE_STRING
/* --- Start of p_string.c --- */
PODEF char*
p_find_char(char *buf, char needle)
{
    while (buf && *buf != needle) buf++;
    return buf;
}

PODEF bool
p_strcmp(const char *a, const char *b)
{
    if (p_strlen(a) != p_strlen(b))
        return false;

    while (a && b && *a != '\0') {
        if (*(a++) != *(b++)) return false;
    }

    return true;
}

PODEF int
p_strlen(const char *a)
{
    int i = 0;
    while (*a++ != '\0') i++;
    return i;
}

PODEF int
p_strtoi(const char *str)
{
    int res = 0;

    while (*str) {
        if (*str >= '0' && *str <= '9') {
            res = res * 10 + (*str - '0');
        } else {
            return res;
        }
        str++;
    }

    return res;
}

PODEF char*
p_strtok(const char *str, char c)
{
    while (*str != c && *str++ != '\0');
    return (*str == c) ? (char*) ++str : 0;
}

PODEF P_StringView
p_strview(const char *cstr) 
{
    P_StringView sv = {0};
    sv.cstr = cstr;
    sv.len = p_strlen(cstr);
    return sv;
}

PODEF void
p_strview_chop_left(P_StringView *sv, u32 n) 
{
    if (n > sv->len) n = sv->len;
    sv->cstr += n;
    sv->len  -= n;
}

PODEF void
p_strview_chop_right(P_StringView *sv, u32 n) 
{
    if (n > sv->len) n = sv->len;
    sv->len  -= n;
}

PODEF P_StringView
p_strview_chop_delim(P_StringView *sv, char delim) 
{
    u32 i = 0;
    while (i < sv->len && sv->cstr[i] != delim) {
        i++;
    }

    P_StringView tok;
    if (i < sv->len) {
        tok.cstr = sv->cstr;
        tok.len  = i; 
        p_strview_chop_left(sv, i + 1);
        return tok;
    }

    tok = *sv;
    p_strview_chop_left(sv, sv->len);
    return tok;
}

PODEF P_StringView
p_strview_chop_type(P_StringView *sv, int(*istype)(int))
{
    u32 i = 0;
    while (i < sv->len && istype(sv->cstr[i])) {
        i++;
    }

    P_StringView tok;
    if (i < sv->len) {
        tok.cstr = sv->cstr;
        tok.len  = i; 
        p_strview_chop_left(sv, i + 1);
        return tok;
    }

    tok = *sv;
    p_strview_chop_left(sv, sv->len);
    return tok;
}


PODEF void
p_strview_trim_left(P_StringView *sv) 
{
    while (sv->len > 0 && p_is_space(sv->cstr[0])) {
        p_strview_chop_left(sv, 1);
    }
}

PODEF void
p_strview_trim_right(P_StringView *sv) 
{
    while (sv->len > 0 && p_is_space(sv->cstr[sv->len-1])) {
        p_strview_chop_right(sv, 1);
    }
}

PODEF void
p_strview_trim(P_StringView *sv) 
{
    p_strview_trim_left(sv);
    p_strview_trim_right(sv);
}

PODEF int 
p_is_space(int c) 
{
    /* Checks for: space, form feed (\f), line feed (\n), 
     * carriage return (\r), horizontal tab (\t), vertical tab (\v) */
    return (c == ' ' || (c >= '\t' && c <= '\r'));
}

PODEF int 
p_is_digit(int c) 
{
    return (c >= '0' && c <= '9');
}

PODEF int 
p_is_alpha(int c) 
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

PODEF int 
p_is_alnum(int c) 
{
    return (p_is_alpha(c) || p_is_digit(c));
}

PODEF int 
p_is_hex(int c) 
{
    return (p_is_digit(c) || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F'));
}

PODEF int 
p_is_upper(int c) 
{
    return (c >= 'A' && c <= 'Z');
}

PODEF int 
p_is_lower(int c) 
{
    return (c >= 'a' && c <= 'z');
}

PODEF int 
p_to_lower(int c) 
{
    if (p_is_upper(c)) return (c + ('a' - 'A'));
    return c;
}

PODEF int 
p_to_upper(int c) 
{
    if (p_is_lower(c)) return (c - ('a' - 'A'));
    return c;
}
/* --- End of p_string.c --- */
#endif

#ifdef P_MODULE_MATH
/* --- Start of p_math.c --- */
PODEF
int p_ceil(float x)
{
    int base = (int) x;
    return (x > base) ? base + 1 : base;
}

PODEF float
p_cosf(float x)
{
    x += 1.57079632679f; 

    while (x > P_PI)  x -= P_PI2;
    while (x < -P_PI) x += P_PI2;

    const float B = 4.0f / P_PI;
    const float C = -4.0f / P_PI_POW2;

    float y = B * x + C * x * (x < 0 ? -x : x);

    const float P = 0.225f;
    y = P * (y * (y < 0 ? -y : y) - y) + y;

    return y;
}

PODEF float
p_sinf(float x)
{
    while (x > P_PI) x -= P_PI2;
    while (x < -P_PI) x += P_PI2;

    const float B = 4.0f / P_PI;
    const float C = -4.0f / (P_PI_POW2);

    float y = B * x + C * x * (x < 0 ? -x : x);

    const float P = 0.225f;
    y = P * (y * (y < 0 ? -y : y) - y) + y;

    return y;
}

PODEF Vec2f
p_vec2f(float x, float y)
{
    return (Vec2f) { .x = x, .y = y };
}

PODEF Vec2f
p_vec2f_add(Vec2f a, Vec2f b)
{
    return (Vec2f) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

PODEF float
p_vec2f_cross(Vec2f a, Vec2f b)
{
    // axby − aybx
    return a.x * b.y - a.y * b.x;
}

PODEF float
p_vec2f_dot(Vec2f a, Vec2f b)
{
    // a · b = ax × bx + ay × by
    return a.x * b.x + a.y * b.y;
}

PODEF Vec2f
p_vec2f_mult(Vec2f a, float b)
{
    return (Vec2f) {
        .x = a.x * b,
        .y = a.y * b,
    };
}

PODEF Vec2f
p_vec2f_sub(Vec2f a, Vec2f b)
{
    return (Vec2f) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

PODEF Vec2i
p_vec2i(float x, float y)
{
    return (Vec2i) { .x = x, .y = y };
}

PODEF Vec2i
p_vec2i_add(Vec2i a, Vec2i b)
{
    return (Vec2i) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

PODEF float
p_vec2i_cross(Vec2i a, Vec2i b)
{
    // axby − aybx
    return a.x * b.y - a.y * b.x;
}

PODEF float
p_vec2i_dot(Vec2i a, Vec2i b)
{
    // a · b = ax × bx + ay × by
    return a.x * b.x + a.y * b.y;
}

PODEF Vec2i
p_vec2i_mult(Vec2i a, float b)
{
    return (Vec2i) {
        .x = a.x * b,
        .y = a.y * b,
    };
}

PODEF Vec2i
p_vec2i_sub(Vec2i a, Vec2i b)
{
    return (Vec2i) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

PODEF Vec3f
p_vec3f(float x, float y, float z)
{
    return (Vec3f) { .x = x, .y = y, .z = z };
}

PODEF P_Mat4
p_mat4_identity(void)
{
    P_Mat4 res = {0};
    res.m[0]  = 1.0f;
    res.m[5]  = 1.0f;
    res.m[10] = 1.0f;
    res.m[15] = 1.0f;
    return res;
}

PODEF P_Mat4
p_mat4_mul(P_Mat4 a, P_Mat4 b)
{
    P_Mat4 res = {0};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            res.m[col * 4 + row] = sum;
        }
    }
    return res;
}

PODEF P_Mat4
p_mat4_translate(Vec3f v)
{
    P_Mat4 res = p_mat4_identity();
    res.m[12] = v.x;
    res.m[13] = v.y;
    res.m[14] = v.z;
    return res;
}

PODEF P_Mat4
p_mat4_translate_by(P_Mat4 m, Vec3f v)
{
    P_Mat4 res = m;
    res.m[12] = m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z + m.m[12];
    res.m[13] = m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z + m.m[13];
    res.m[14] = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14];
    res.m[15] = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15];
    return res;
}

PODEF P_Mat4
p_mat4_rotate_x(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[5]  =  c;
    res.m[6]  =  s;
    res.m[9]  = -s;
    res.m[10] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_rotate_x_by(P_Mat4 m, float rad)
{
    return p_mat4_mul(m, p_mat4_rotate_x(rad));
}

PODEF P_Mat4
p_mat4_rotate_y(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[0]  =  c;
    res.m[2]  = -s;
    res.m[8]  =  s;
    res.m[10] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_rotate_y_by(P_Mat4 m, float rad)
{
    return p_mat4_mul(m, p_mat4_rotate_y(rad));
}

PODEF P_Mat4
p_mat4_rotate_z(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[0] =  c;
    res.m[1] =  s;
    res.m[4] = -s;
    res.m[5] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_perspective(float fov_rad, float aspect, float near_z, float far_z)
{
    P_Mat4 res = {0};
    float tan_half_fov = p_sinf(fov_rad * 0.5f) / p_cosf(fov_rad * 0.5f);

    res.m[0]  = 1.0f / (aspect * tan_half_fov);
    res.m[5]  = 1.0f / tan_half_fov;
    res.m[10] = far_z / (near_z - far_z);
    res.m[11] = -1.0f;
    res.m[14] = (near_z * far_z) / (near_z - far_z);
    return res;
}
/* --- End of p_math.c --- */
#endif

#ifdef P_MODULE_ALLOC
/* --- Start of p_allocators.c --- */
PODEF bool
is_power_of_two(uintptr x) {
	return (x & (x-1)) == 0;
}

PODEF uintptr
align_forward(uintptr ptr, usize align) 
{
    uintptr p, a, remainder;
    assert(is_power_of_two(align));

    p = ptr;
    a = (uintptr)align;
	remainder = p & (a-1);

    return (remainder != 0) ? p + a - remainder : p;
}

PODEF void
p_arena_create(Arena *a, void *backing_buffer, usize backing_buffer_length) 
{
	a->buf = (unsigned char *)backing_buffer;
	a->buf_len = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

PODEF void*
p_arena_alloc_align(Arena *a, usize s, usize align) 
{

	// Align 'curr_offset' forward to the specified alignment
	uintptr curr_ptr = (uintptr)a->buf + (uintptr)a->curr_offset;
	uintptr offset = align_forward(curr_ptr, align);
	offset -= (uintptr)a->buf; // Change to relative offset

	// Check to see if the backing memory has space left
	if (offset+s <= a->buf_len) {
		void *ptr = &a->buf[offset];
		a->prev_offset = offset;
		a->curr_offset = offset+s;

		memset(ptr, 0, s);
		return ptr;
	}

    return NULL;
}

PODEF void*
p_arena_alloc(Arena *a, usize s) {
    return p_arena_alloc_align(a, s, DEFAULT_ALIGNMENT);
}

PODEF void*
p_arena_resize_align(Arena *a, void *old_memory, usize old_size, usize new_size, usize align)
{
    assert(is_power_of_two(align));
    unsigned char *old_mem = (unsigned char *)old_memory;

    if (old_mem == NULL || old_size == 0) {
        return p_arena_alloc_align(a, new_size, align);
    } else if (a->buf <= old_mem && old_mem < a->buf+a->buf_len) {
        if (a->buf+a->prev_offset == old_mem) {
            a->curr_offset = a->prev_offset + new_size;
            if (new_size > old_size) {
                // Zero the new memory by default
                memset(&a->buf[a->curr_offset], 0, new_size-old_size);
            }
            return old_memory;
        } else {
            void *new_memory = p_arena_alloc_align(a, new_size, align);
            size_t copy_size = old_size < new_size ? old_size : new_size;
            memmove(new_memory, old_memory, copy_size);
            return new_memory;
        }

    } else {
        assert(0 && "Memory is out of bounds of the buffer in this arena");
        return NULL;
    }
}

PODEF void*
p_arena_resize(Arena *a, void *old_memory, usize old_size, usize new_size)
{
    return p_arena_resize_align(a, old_memory, old_size, new_size, DEFAULT_ALIGNMENT);
}

PODEF void
p_arena_free_all(Arena *a)
{
    a->curr_offset = 0;
    a->prev_offset = 0;
}

PODEF void
p_arena_destroy(Arena* a) 
{
    a->buf_len = 0;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

PODEF void
p_stack_create(Stack *s, void *backing_buffer, usize backing_buffer_length)
{
    s->buf = (unsigned char*) backing_buffer;
    s->buf_len = backing_buffer_length;
    s->offset = 0;
}

PODEF usize
calc_padding_with_header(uintptr ptr, uintptr alignment, usize header_size) 
{
	uintptr p, a, modulo, padding, needed_space;

	assert(is_power_of_two(alignment));

	p = ptr;
	a = alignment;
	modulo = p & (a-1); // (p % a) as it assumes alignment is a power of two

	padding = 0;
	needed_space = 0;

	if (modulo != 0) { // Same logic as 'align_forward'
		padding = a - modulo;
	}

	needed_space = (uintptr)header_size;

	if (padding < needed_space) {
		needed_space -= padding;

		if ((needed_space & (a-1)) != 0) {
			padding += a * (1+(needed_space/a));
		} else {
			padding += a * (needed_space/a);
		}
	}

	return (usize)padding;
}

PODEF void*
p_stack_alloc(Stack *s, usize len, usize alignment) 
{
	uintptr curr_addr, next_addr;
	usize padding;
	Stack_Allocation_Header *header;

	assert(is_power_of_two(alignment));

	if (alignment > 128) {
		// As the padding is 8 bits (1 byte), the largest alignment that can
		// be used is 128 bytes
		alignment = 128;
	}

	curr_addr = (uintptr)s->buf + (uintptr)s->offset;
	padding = calc_padding_with_header(curr_addr, (uintptr)alignment, sizeof(Stack_Allocation_Header));
	if (s->offset + padding + len > s->buf_len) {
		// Stack allocator is out of memory
		return NULL;
	}
	s->offset += padding;

	next_addr = curr_addr + (uintptr)padding;
	header = (Stack_Allocation_Header *)(next_addr - sizeof(Stack_Allocation_Header));
	header->padding = (u8)padding;

	s->offset += len;

	return memset((void *)next_addr, 0, len);
}

PODEF void
p_stack_free(Stack *s, void *ptr) 
{
	if (ptr != NULL) {
		uintptr start, end, curr_addr;
		Stack_Allocation_Header *header;
		usize prev_offset;

		start = (uintptr)s->buf;
		end = start + (uintptr)s->buf_len;
		curr_addr = (uintptr)ptr;

		if (!(start <= curr_addr && curr_addr < end)) {
			assert(0 && "Out of bounds memory address passed to stack allocator (free)");
			return;
		}

		if (curr_addr >= start+(uintptr)s->offset) {
			// Allow double frees
			return;
		}

		header = (Stack_Allocation_Header *)(curr_addr - sizeof(Stack_Allocation_Header));
		prev_offset = (usize)(curr_addr - (uintptr)header->padding - start);

		s->offset = prev_offset;
	}
}

PODEF void
p_stack_free_all(Stack* s)
{
    s->offset = 0;
}

PODEF void
p_stack_destroy(Stack* s)
{
    s->buf = 0;
    s->buf_len = 0;
    s->offset = 0;
}

PODEF void
p_pool_create(Pool *p, void *buf, usize buf_len, usize chunk_size, usize chunk_align) 
{
    /* align start */
    uintptr start = (uintptr) buf;
    uintptr aligned = align_forward(start, chunk_align);
    buf_len -= (aligned - start);

    /* align chunk usize */
    chunk_size = (usize) align_forward(chunk_size, chunk_align); 

	assert(chunk_size >= sizeof(Pool_Free_Node) && "Chunk usize is too small");
	assert(buf_len >= chunk_size && "Backing buffer length is smaller than the chunk size");
    
    p->buf = (unsigned char *) buf;
    p->pool_size = buf_len;
    p->chunk_size = chunk_size;
    p->head = NULL;

    p_pool_free_all(p);
}

PODEF void*
p_pool_alloc(Pool *p)
{
    Pool_Free_Node *node = p->head;

    assert(node != NULL && "Pool allocator has no free memory");
    
    p->head = p->head->next;

    return memset(node, 0 , p->chunk_size);
}

PODEF void 
p_pool_free(Pool *p, void *ptr) 
{
    Pool_Free_Node *node;

	void *start = p->buf;
	void *end = &p->buf[p->pool_size];

	if (ptr == NULL) {
		return;
	}

	if (!(start <= ptr && ptr < end)) {
		assert(0 && "Memory is out of bounds of the buffer in this pool");
		return;
	}

	node = (Pool_Free_Node *)ptr;
    node->next = p->head;
	p->head = node;
}

PODEF void 
p_pool_free_all(Pool *p) 
{
	usize chunk_count = p->pool_size / p->chunk_size;
	usize i;

	for (i = 0; i < chunk_count; i++) {
		void *ptr = &p->buf[i * p->chunk_size];
		Pool_Free_Node *node = (Pool_Free_Node *)ptr;

		// Push free node onto thte free list
		node->next = p->head;
		p->head = node;
	}
}

PODEF void 
p_pool_destroy(Pool *p) 
{
    p->buf = NULL;
    p->pool_size = 0;
    p->chunk_size = 0;
    p->head = NULL;
}

/* --- End of p_allocators.c --- */
#endif

#define P_MAX_PRINTF_SIZE 1024
PODEF void
p_log_printf(P_Log_Level level, const char* src, ...)
{
    static char out_message[P_MAX_PRINTF_SIZE];
    static u32 offset = P_PREFIX_LEN + 1;

    memcpy(out_message, p_prefix[level], P_PREFIX_LEN); /* prefixes have a fixed len */
    out_message[P_PREFIX_LEN] = ' '; /* write space */

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, src);
    int len = vsnprintf(out_message+offset, P_MAX_PRINTF_SIZE-offset, src, arg_ptr); // append message
    va_end(arg_ptr);

    if (len < 0) len = 0;
    usize total_len = offset + len;
    if (total_len > P_MAX_PRINTF_SIZE) {
        total_len = P_MAX_PRINTF_SIZE;
    }

    p_stdout(out_message, total_len);
    p_stdout("\n", 1);
}

#endif // PODIUM_IMPLEMENTATION
