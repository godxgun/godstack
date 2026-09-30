/* t_otf.c - OpenType / CFF faces. Copyright (c) 2025-2026 Vasco Alves
 * CFF outlines are not implemented yet.
 */

#include "type.h"

size_t
type_otf_memory(void)
{
	return 0;
}

TypeOTF *
type_otf_place(void *buf, size_t bufsize, const void *bytes, size_t len)
{
	(void)buf;
	(void)bufsize;
	(void)bytes;
	(void)len;
	return NULL;
}

TypeError
type_otf_error(const TypeOTF *font)
{
	(void)font;
	return TYPE_ERR_UNIMPLEMENTED;
}

int
type_otf_outline(const TypeOTF *font)
{
	(void)font;
	return 0;
}

int
type_otf_color(const TypeOTF *font)
{
	(void)font;
	return 0;
}

TypeFont *
type_font_from_otf(void *buf, size_t bufsize, const TypeOTF *otf, const TypeFaceParams *face)
{
	(void)buf;
	(void)bufsize;
	(void)otf;
	(void)face;
	return NULL;
}
