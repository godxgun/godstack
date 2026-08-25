/*
 * Logging!
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define PEAK_MAX_PRINTF 1024
#define PEAK_MAX_ALLOCS 512

typedef struct {
    void *ptr;
    size_t size;
    const char *file;
    const char *func;
    int line;
} PeakDebugMemoryInfo;

static uint64_t peak_alloc_count = 0;
static PeakDebugMemoryInfo peak_ptr_array[PEAK_MAX_ALLOCS];

void
peak_log_printf(PeakLogLevel level, const char *src, ...)
{
    char out[PEAK_MAX_PRINTF];
    va_list ap;
    int len;
    size_t offset = P_PREFIX_LEN + 1;

    if (level < 0 || level >= P_COUNT_LOG_LEVEL)
        level = P_LOG_LEVEL_ERROR;
    memcpy(out, p_prefix[level], P_PREFIX_LEN);
    out[P_PREFIX_LEN] = ' ';
    va_start(ap, src);
    len = vsnprintf(out + offset, PEAK_MAX_PRINTF - offset, src, ap);
    va_end(ap);
    if (len < 0) len = 0;
    if (offset + (size_t)len >= PEAK_MAX_PRINTF)
        len = (int)(PEAK_MAX_PRINTF - offset - 1);
    out[offset + (size_t)len] = '\n';
    fwrite(out, 1, offset + (size_t)len + 1, (level <= P_LOG_LEVEL_ERROR) ? stderr : stdout);
}

void *
peak_debug_malloc_impl(size_t size, const char *file, int line, const char *func)
{
    void *ptr = malloc(size);
    printf("[ALLOC] %p (%zu bytes) -> %s:%d %s()\n", ptr, size, file, line, func);

    if (ptr) {
        if (peak_alloc_count < PEAK_MAX_ALLOCS) {
            peak_ptr_array[peak_alloc_count++] = (PeakDebugMemoryInfo){
                .ptr = ptr,
                .size = size,
                .file = file,
                .func = func,
                .line = line
            };
        } else {
            fprintf(stderr, "[ERROR] Debug allocator tracking capacity (%d) exceeded!\n", PEAK_MAX_ALLOCS);
        }
    }
    return ptr;
}

void
peak_debug_free_impl(void *ptr, const char *file, int line, const char *func)
{
    printf("[FREE]  %p -> %s:%d %s()\n", ptr, file, line, func);

    if (!ptr) return;

    bool found = false;
    uint64_t index = 0;

    for (index = 0; index < peak_alloc_count; ++index) {
        if (peak_ptr_array[index].ptr == ptr) {
            found = true;
            break;
        }
    }

    if (found) {
        for (uint64_t j = index; j < peak_alloc_count - 1; ++j) {
            peak_ptr_array[j] = peak_ptr_array[j + 1];
        }
        peak_alloc_count--;
    } else {
        fprintf(stderr, "[WARNING] Attempted to free untracked/double-freed pointer %p at %s:%d %s()\n",
                ptr, file, line, func);
    }

    free(ptr);
}

void *
peak_debug_realloc_impl(void *ptr, size_t size, const char *file, int line, const char *func)
{
    if (!ptr) {
        return peak_debug_malloc_impl(size, file, line, func);
    }
    if (size == 0) {
        peak_debug_free_impl(ptr, file, line, func);
        return NULL;
    }

    uintptr_t old_addr = (uintptr_t)ptr;
    void *new_ptr = realloc(ptr, size);
    printf("[REALLOC] %p -> %p (%zu bytes) -> %s:%d %s()\n", (void *)old_addr, new_ptr, size, file, line, func);

    if (new_ptr) {
        bool found = false;
        for (uint64_t i = 0; i < peak_alloc_count; ++i) {
            if (peak_ptr_array[i].ptr == (void *)old_addr) {
                peak_ptr_array[i].ptr = new_ptr;
                peak_ptr_array[i].size = size;
                peak_ptr_array[i].file = file;
                peak_ptr_array[i].line = line;
                peak_ptr_array[i].func = func;
                found = true;
                break;
            }
        }
        if (!found) {
            if (peak_alloc_count < PEAK_MAX_ALLOCS) {
                peak_ptr_array[peak_alloc_count++] = (PeakDebugMemoryInfo){
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

void
peak_debug_memory_report(void)
{
    printf("\n==================== MEMORY REPORT ====================\n");
    printf("Remaining unfreed allocations: %lu\n", (unsigned long)peak_alloc_count);

    for (uint64_t i = 0; i < peak_alloc_count; ++i) {
        PeakDebugMemoryInfo *info = &peak_ptr_array[i];
        printf("[LEAK] %p (%zu bytes) allocated at %s:%d in %s()\n",
               info->ptr, info->size, info->file, info->line, info->func);
    }
    printf("=======================================================\n");
}
