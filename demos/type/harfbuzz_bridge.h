/* Experimental adapter, NOT an arena-qualified application dependency.
 * hb_font/hb_buffer are caller-owned but HarfBuzz may allocate internally.
 * Include Type.h, hb.h and hb-ot.h before this header. This adapter shapes one
 * already-itemized logical run, not a paragraph or fallback strategy. */
#ifndef TYPE_DEMO_HARFBUZZ_BRIDGE_H
#define TYPE_DEMO_HARFBUZZ_BRIDGE_H
#include <limits.h>

static TypeError type_demo_hb_run(hb_font_t *font, hb_buffer_t *buffer, uint32_t font_id,
                                  const char *text, size_t len, size_t start, size_t end,
                                  hb_direction_t direction, hb_script_t script,
                                  const char *language, TypeShapedGlyph *out, uint32_t cap,
                                  uint32_t *written)
{
	unsigned int count, i;
	hb_glyph_info_t *info;
	hb_glyph_position_t *pos;
	hb_feature_t liga = {HB_TAG('l', 'i', 'g', 'a'), 1, 0, (unsigned int)-1};
	if (written)
		*written = 0;
	if (!font || !buffer || len > INT_MAX || start > end || end > len ||
	    !HB_DIRECTION_IS_HORIZONTAL(direction) || (!out && cap))
		return TYPE_ERR_BUF_TOO_SMALL;
	if (!type_utf8_validate(text, len) ||
	    !type_utf8_validate(text ? text + start : NULL, end - start))
		return TYPE_ERR_INVALID_UTF8;
	hb_buffer_clear_contents(buffer);
	hb_buffer_set_direction(buffer, direction);
	hb_buffer_set_script(buffer, script);
	hb_buffer_set_language(buffer, hb_language_from_string(language, -1));
	hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
	if (end == start)
		return TYPE_OK;
	hb_buffer_add_utf8(buffer, text, (int)len, (unsigned int)start, (int)(end - start));
	hb_shape(font, buffer, &liga, 1);
	if (!hb_buffer_allocation_successful(buffer))
		return TYPE_ERR_BUF_TOO_SMALL;
	info = hb_buffer_get_glyph_infos(buffer, &count);
	pos = hb_buffer_get_glyph_positions(buffer, NULL);
	/* Capacity failure is all-or-nothing for this adapter. */
	if (count > cap)
		return TYPE_ERR_BUF_TOO_SMALL;
	for (i = 0; i < count; i++) {
		out[i].font_id = font_id;
		out[i].glyph_id = info[i].codepoint;
		out[i].cluster = info[i].cluster; /* absolute logical UTF-8 byte offset */
		out[i].x_advance = (float)pos[i].x_advance / 64.0f;
		out[i].y_advance = -(float)pos[i].y_advance / 64.0f;
		out[i].x_offset = (float)pos[i].x_offset / 64.0f;
		out[i].y_offset = -(float)pos[i].y_offset / 64.0f;
	}
	if (written)
		*written = count;
	return TYPE_OK;
}
#endif
