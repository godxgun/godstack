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
