/* MIT License; see LICENSE. */
#ifndef TYPE_TEST_SUPPORT_H
#define TYPE_TEST_SUPPORT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *test_fonts[] = {"NotoSans-Regular.ttf",       "NotoSans-Bold.ttf",
                                   "NotoSansHebrew-Regular.ttf", "NotoSansHebrew-Bold.ttf",
                                   "NotoSansArabic-Regular.ttf", "NotoSansArabic-Bold.ttf",
                                   "LiberationSans-Regular.ttf"};
static inline void *test_align(void *p)
{
    return (void *)(((uintptr_t)p + 15u) & ~(uintptr_t)15u);
}
static inline size_t test_read(const char *path, unsigned char *bytes, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    assert(f);
    n = fread(bytes, 1, cap, f);
    assert(!ferror(f) && fgetc(f) == EOF);
    assert(!fclose(f));
    return n;
}
#endif
