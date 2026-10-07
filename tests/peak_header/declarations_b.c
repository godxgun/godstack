#include "peak.h"
#include "peak.h"

int declaration_b_alloc(void);

int
declaration_b_alloc(void)
{
    void *buffer;

    buffer = peak_aligned_alloc(32, 16);
    if (!buffer) return 0;
    peak_aligned_free(buffer);
    return 1;
}
