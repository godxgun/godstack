#include "Peak.h"
#include "Peak.h"

int declaration_b_alloc(void);

int
declaration_b_alloc(void)
{
    void *buffer;

    if (peak_aligned_alloc(0, 16)) return 0;
    buffer = peak_aligned_alloc(33, 16);
    if (!buffer) return 0;
    if ((uintptr_t)buffer % 16) {
        peak_aligned_free(buffer);
        return 0;
    }
    peak_aligned_free(buffer);
    return 1;
}
