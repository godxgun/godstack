/* Standalone test storage, not Sigma's runtime allocator integration. */
#ifndef TYPE_FIXTURE_SUPPORT_H
#define TYPE_FIXTURE_SUPPORT_H
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void *fixture_align(void *p)
{
	return (void *)(((uintptr_t)p + 15u) & ~(uintptr_t)15u);
}

static size_t fixture_font(const char *path, unsigned char *bytes, size_t cap)
{
	FILE *f = fopen(path, "rb");
	size_t n;
	assert(f);
	n = fread(bytes, 1, cap, f);
	assert(!ferror(f));
	assert(fgetc(f) == EOF); /* refuse oversized fixture input */
	assert(fclose(f) == 0);
	assert(n > 0);
	return n;
}

static TypeCtx *fixture_ctx(unsigned char *storage, size_t cap)
{
	TypeParams p;
	type_params_default(&p);
	p.atlas_width = p.atlas_height = 512;
	p.max_faces = 3;
	p.cache_capacity = 128;
	assert(type_memory(&p) > 0 && type_memory(&p) <= cap - 15);
	return type_place(fixture_align(storage), cap - 15, &p);
}
#endif
