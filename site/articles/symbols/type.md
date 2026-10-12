+++
title = 'Type symbols'
description = 'Types, macros, and functions declared by Type.h.'
weight = 70
[params]
  id = 'symbols/type'
  product = 'Type'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Type symbols

For setup and API workflows, read the [Type guide](/docs/type/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Type.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### TypeTTF {#typettf}

Source: `Type.h:84`

```c
typedef struct TypeTTF TypeTTF;
```

Parsed sfnt face: glyf, CFF or CFF2.

#### TypeOTF {#typeotf}

Source: `Type.h:85`

```c
typedef struct TypeOTF TypeOTF;
```

OpenType placement view.

#### TypeFont {#typefont}

Source: `Type.h:86`

```c
typedef struct TypeFont TypeFont;
```

Face view: parsed file + weight + style. Borrows the file.

#### TypeAtlas {#typeatlas}

Source: `Type.h:87`

```c
typedef struct TypeAtlas TypeAtlas;
```

Shelf-packed coverage atlas.

#### TypeAtlasRgba {#typeatlasrgba}

Source: `Type.h:88`

```c
typedef struct TypeAtlasRgba TypeAtlasRgba;
```

#### TypeCtx {#typectx}

Source: `Type.h:89`

```c
typedef struct TypeCtx TypeCtx;
```

Faces, fallbacks, atlases, glyph LRU.

#### TypeError {#typeerror}

Source: `Type.h:91`

```c
typedef enum {
	TYPE_OK = 0,
	TYPE_ERR_INVALID_FONT,
	TYPE_ERR_UNSUPPORTED_TABLE,
	TYPE_ERR_GLYPH_NOT_FOUND,
	TYPE_ERR_ATLAS_FULL,
	TYPE_ERR_INVALID_UTF8,
	TYPE_ERR_UNIMPLEMENTED,
	TYPE_ERR_BUF_TOO_SMALL,
	TYPE_ERR_SCRATCH_REQUIRED,
	TYPE_ERR_SCRATCH_FULL,
	TYPE_ERR_UNTRUSTED_FONT, /* retained enum value for source compatibility */
	TYPE_ERR_LIMIT,
} TypeError;
```

#### TypeScratch {#typescratch}

Source: `Type.h:110`

```c
typedef struct TypeScratch {
	uint8_t *data;
	size_t capacity, used, peak;
	int failed;
} TypeScratch;
```

Caller-owned temporary-arena view. No allocation or heap fallback.
Initialize once per borrowed slice. Operations restore used, retain peak and
failed diagnostics. Do not share a view across concurrent/reentrant calls.
data must not overlap context, font bytes, output or bitmap storage.

#### TypeAntiAlias {#typeantialias}

Source: `Type.h:120`

```c
typedef enum {
	TYPE_AA_NONE = 0, /* binary coverage */
	TYPE_AA_BOX = 1,  /* box-filter antialiasing */
	TYPE_AA_SDF = 2,  /* signed distance; raster returns TYPE_ERR_UNIMPLEMENTED */
} TypeAntiAlias;
```

#### TypeWeight {#typeweight}

Source: `Type.h:127`

```c
typedef enum {
	TYPE_WEIGHT_THIN = 100,
	TYPE_WEIGHT_EXTRA_LIGHT = 200,
	TYPE_WEIGHT_LIGHT = 300,
	TYPE_WEIGHT_REGULAR = 400,
	TYPE_WEIGHT_MEDIUM = 500,
	TYPE_WEIGHT_SEMI_BOLD = 600,
	TYPE_WEIGHT_BOLD = 700,
	TYPE_WEIGHT_EXTRA_BOLD = 800,
	TYPE_WEIGHT_BLACK = 900,
} TypeWeight;
```

CSS-ish usWeightClass. Any u16 is legal; these are the named stops.

#### TypeWidth {#typewidth}

Source: `Type.h:140`

```c
typedef enum {
	TYPE_WIDTH_NARROW = 0,
	TYPE_WIDTH_WIDE,
	TYPE_WIDTH_AMBIGUOUS,
	TYPE_WIDTH_NEUTRAL,
} TypeWidth;
```

UAX #11. Neutral is the default outside the wide and ambiguous ranges.

#### TypeStyle {#typestyle}

Source: `Type.h:147`

```c
typedef struct TypeStyle {
	uint8_t italic;
	uint8_t bold;
} TypeStyle;
```

#### TypeFaceParams {#typefaceparams}

Source: `Type.h:153`

```c
typedef struct TypeFaceParams {
	uint16_t weight;
	TypeStyle style;
} TypeFaceParams;
```

weight 0 means "unset": bold style selects TYPE_WEIGHT_BOLD, otherwise REGULAR.

#### TypeParams {#typeparams}

Source: `Type.h:164`

```c
typedef struct TypeParams {
	uint32_t atlas_width;
	uint32_t atlas_height;
	uint32_t cache_capacity;
	uint32_t max_faces;
	uint32_t max_fallbacks;
	uint8_t ligatures;
	uint8_t color_emoji;
	uint8_t antialias;
} TypeParams;
```

atlas_* and cache_capacity must be > 0 (type_params_default fills the
velocitty defaults). max_faces and max_fallbacks bound storage inside the
context buffer. ligatures/color_emoji are reserved; the unshaped helper
applies neither GSUB nor color glyphs. antialias is TypeAntiAlias.

#### TypeMetrics {#typemetrics}

Source: `Type.h:175`

```c
typedef struct TypeMetrics {
	float ascender;
	float descender;
	float line_gap;
	uint16_t units_per_em;
} TypeMetrics;
```

#### TypeBitmap {#typebitmap}

Source: `Type.h:185`

```c
typedef struct TypeBitmap {
	uint16_t width;
	uint16_t height;
	int16_t bearing_x;
	int16_t bearing_y;
	uint16_t advance;
	uint8_t *pixels;
	uint8_t color;
} TypeBitmap;
```

pixels is coverage (1 byte) or straight RGBA8 (4 bytes) when color != 0.
advance is pixels at the requested size. Bearing is the bitmap origin
relative to the pen, y up.

#### TypeGlyph {#typeglyph}

Source: `Type.h:195`

```c
typedef struct TypeGlyph {
	uint32_t font_id;
	uint16_t glyph_id;
	float advance;
	float bearing_x;
	float bearing_y;
	uint16_t width;
	uint16_t height;
	uint16_t atlas_x;
	uint16_t atlas_y;
	uint8_t color; /* 0 coverage atlas, 1 color atlas */
} TypeGlyph;
```

#### TypeCell {#typecell}

Source: `Type.h:208`

```c
typedef struct TypeCell {
	TypeGlyph glyph;
	float x;
	float y;
	uint32_t codepoint;
} TypeCell;
```

#### TypeShapedGlyph {#typeshapedglyph}

Source: `Type.h:220`

```c
typedef struct TypeShapedGlyph {
	uint32_t font_id, glyph_id, cluster;
	float x_advance, y_advance, x_offset, y_offset;
} TypeShapedGlyph;
```

External shaper output, in visual glyph order. cluster is a UTF-8 byte
offset in the caller's logical text, never a glyph index. Positions are
pixels, fractional, y down (negate HarfBuzz y values on import). font_id
and glyph_id must refer to exactly the face shaped, including glyph 0.
Run size uses the same integer-pixel rounding as type_glyph.

#### TypePositionedGlyph {#typepositionedglyph}

Source: `Type.h:225`

```c
typedef struct TypePositionedGlyph {
	TypeGlyph glyph;
	uint32_t cluster;
	float x, y;
} TypePositionedGlyph;
```

#### TypeTextMetrics {#typetextmetrics}

Source: `Type.h:233`

```c
typedef struct TypeTextMetrics {
	float advance_x, advance_y;
	float min_x, min_y, max_x, max_y;
	uint8_t has_ink;
} TypeTextMetrics;
```

Ink bounds relative to initial baseline; advance includes invisible glyphs.
has_ink == 0 means all bounds are zero.

#### TypeGlyphKey {#typeglyphkey}

Source: `Type.h:239`

```c
typedef struct TypeGlyphKey {
	uint32_t font_id;
	uint32_t glyph_id;
	uint16_t size_px;
	uint16_t flags; /* TYPE_STYLE_BOLD | TYPE_STYLE_ITALIC */
} TypeGlyphKey;
```

#### TypeGlyphStats {#typeglyphstats}

Source: `Type.h:246`

```c
typedef struct TypeGlyphStats {
	uint64_t hits;
	uint64_t misses;
	uint64_t raster_ns;
	uint64_t atlas_ns;
} TypeGlyphStats;
```

#### TypeRect {#typerect}

Source: `Type.h:253`

```c
typedef struct TypeRect {
	uint32_t x, y, w, h;
} TypeRect;
```

#### TypeVertex {#typevertex}

Source: `Type.h:316`

```c
typedef struct TypeVertex {
	double x, y, cx, cy, cx1, cy1;
	uint8_t type;
} TypeVertex;
```

### Functions

#### type_scratch_init {#type_scratch_init}

Source: `Type.h:116`

```c
void type_scratch_init(TypeScratch *scratch, void *data, size_t capacity);
```

#### type_set_scratch {#type_set_scratch}

Source: `Type.h:118`

```c
void type_set_scratch(TypeCtx *ctx, TypeScratch *scratch);
```

Borrow until detached (NULL). No operation may overlap an arena reset.

#### type_params_default {#type_params_default}

Source: `Type.h:257`

```c
void type_params_default(
    TypeParams *params);
```

1024^2 atlas, 2048 cache, 8 faces, ligatures, color emoji, box AA.

#### type_face_params_default {#type_face_params_default}

Source: `Type.h:259`

```c
void type_face_params_default(TypeFaceParams *face);
```

weight unset, roman.

#### type_ttf_memory {#type_ttf_memory}

Source: `Type.h:266`

```c
size_t type_ttf_memory(void);
```

---------------------------------------------------------------------------
Parsed files. Bytes are borrowed for the life of the placed object.
Type.h implementation
-------------------------------------------------------------------------

#### type_otf_memory {#type_otf_memory}

Source: `Type.h:267`

```c
size_t type_otf_memory(void);
```

#### type_ttf_place_index {#type_ttf_place_index}

Source: `Type.h:274`

```c
TypeTTF *type_ttf_place_index(void *buf, size_t bufsize, const void *bytes, size_t len,
			      uint32_t face_index);
```

All admission paths validate bounded table views. Nested outlines are checked
on access and may return TYPE_ERR_INVALID_FONT, TYPE_ERR_UNSUPPORTED_TABLE or
TYPE_ERR_LIMIT. Font bytes must remain immutable. Placement is 16-byte aligned.
The trusted suffix is retained as an alias, without bypassing validation.
Both placement types accept glyf/CFF/CFF2. TTC chooses face zero by default.

#### type_ttf_place {#type_ttf_place}

Source: `Type.h:276`

```c
TypeTTF *type_ttf_place(void *buf, size_t bufsize, const void *bytes, size_t len);
```

#### type_ttf_place_trusted {#type_ttf_place_trusted}

Source: `Type.h:277`

```c
TypeTTF *type_ttf_place_trusted(void *buf, size_t bufsize, const void *bytes, size_t len);
```

#### type_otf_place {#type_otf_place}

Source: `Type.h:278`

```c
TypeOTF *type_otf_place(void *buf, size_t bufsize, const void *bytes, size_t len);
```

#### type_ttf_error {#type_ttf_error}

Source: `Type.h:280`

```c
TypeError type_ttf_error(
    const TypeTTF *font);
```

TYPE_OK, or why place failed. place(NULL buf) is not required.

#### type_otf_error {#type_otf_error}

Source: `Type.h:282`

```c
TypeError type_otf_error(const TypeOTF *font);
```

#### type_ttf_outline {#type_ttf_outline}

Source: `Type.h:284`

```c
int type_ttf_outline(const TypeTTF *font);
```

1 when glyf outlines are usable.

#### type_otf_outline {#type_otf_outline}

Source: `Type.h:285`

```c
int type_otf_outline(const TypeOTF *font);
```

#### type_ttf_color {#type_ttf_color}

Source: `Type.h:286`

```c
int type_ttf_color(const TypeTTF *font);
```

1 when a CBDT/CBLC face was parsed.

#### type_otf_color {#type_otf_color}

Source: `Type.h:287`

```c
int type_otf_color(const TypeOTF *font);
```

#### type_font_memory {#type_font_memory}

Source: `Type.h:293`

```c
size_t type_font_memory(void);
```

---------------------------------------------------------------------------
Face. Borrows the placed file; does not copy glyph bytes.
-------------------------------------------------------------------------

#### type_font_from_ttf {#type_font_from_ttf}

Source: `Type.h:295`

```c
TypeFont *type_font_from_ttf(void *buf, size_t bufsize, const TypeTTF *ttf,
			     const TypeFaceParams *face);
```

#### type_font_from_otf {#type_font_from_otf}

Source: `Type.h:297`

```c
TypeFont *type_font_from_otf(void *buf, size_t bufsize, const TypeOTF *otf,
			     const TypeFaceParams *face);
```

#### type_font_weight {#type_font_weight}

Source: `Type.h:300`

```c
uint16_t type_font_weight(const TypeFont *font);
```

#### type_font_style {#type_font_style}

Source: `Type.h:301`

```c
TypeStyle type_font_style(const TypeFont *font);
```

#### type_font_metrics {#type_font_metrics}

Source: `Type.h:303`

```c
TypeError type_font_metrics(const TypeFont *font, float size_px, TypeMetrics *out);
```

#### type_font_glyph_index {#type_font_glyph_index}

Source: `Type.h:304`

```c
TypeError type_font_glyph_index(
    const TypeFont *font, uint32_t codepoint,
    uint16_t *out_gid);
```

TYPE_ERR_GLYPH_NOT_FOUND when unmapped. .notdef is not a hit.

#### type_font_advance {#type_font_advance}

Source: `Type.h:307`

```c
TypeError type_font_advance(const TypeFont *font, uint16_t glyph_id,
			    uint16_t *out_fu);
```

font units

#### type_font_has_drawable {#type_font_has_drawable}

Source: `Type.h:309`

```c
int type_font_has_drawable(const TypeFont *font, uint16_t glyph_id);
```

1 outline or color bitmap

#### type_font_outline {#type_font_outline}

Source: `Type.h:320`

```c
TypeError type_font_outline(const TypeFont *font, uint16_t glyph_id, TypeScratch *scratch,
			    TypeVertex *vertices, uint32_t capacity, uint32_t *count);
```

#### type_font_name {#type_font_name}

Source: `Type.h:324`

```c
TypeError type_font_name(const TypeFont *font, uint16_t name_id, const void **bytes, size_t *len,
			 uint16_t *platform);
```

Name bytes are borrowed, in the encoding identified by platform (typically
platform 3 = UTF-16BE). No decoding or allocation is performed.

#### type_font_vertical_advance {#type_font_vertical_advance}

Source: `Type.h:326`

```c
TypeError type_font_vertical_advance(const TypeFont *font, uint16_t glyph_id, uint16_t *advance,
				     int16_t *bearing);
```

#### type_font_bitmap_info {#type_font_bitmap_info}

Source: `Type.h:330`

```c
TypeError type_font_bitmap_info(const TypeFont *font, uint16_t glyph_id, float size_px,
				TypeBitmap *out);
```

Fill everything except pixels. Use type_bitmap_bytes to size the caller buffer.

#### type_bitmap_bytes {#type_bitmap_bytes}

Source: `Type.h:332`

```c
size_t type_bitmap_bytes(const TypeBitmap *info);
```

width*height, or *4 when color

#### type_font_render_bitmap {#type_font_render_bitmap}

Source: `Type.h:339`

```c
TypeError type_font_render_bitmap(const TypeFont *font, uint16_t glyph_id, float size_px,
				  TypeAntiAlias aa, void *pixels, size_t len, TypeBitmap *out);
```

pixels must hold type_bitmap_bytes of the info at this size. Empty glyphs
(width or height 0) succeed and leave pixels untouched. On raster scratch
exhaustion coverage is zeroed and out is not published.
Legacy entry point needs no scratch for empty glyphs; otherwise returns
TYPE_ERR_SCRATCH_REQUIRED. Use the explicit-scratch variant for rendering.

#### type_font_render_bitmap_scratch {#type_font_render_bitmap_scratch}

Source: `Type.h:341`

```c
TypeError type_font_render_bitmap_scratch(const TypeFont *font, uint16_t glyph_id, float size_px,
					  TypeAntiAlias aa, void *pixels, size_t len,
					  TypeScratch *scratch, TypeBitmap *out);
```

#### type_font_render_sdf {#type_font_render_sdf}

Source: `Type.h:344`

```c
TypeError type_font_render_sdf(const TypeFont *font, uint16_t glyph_id, float size_px, void *pixels,
			       size_t len, TypeBitmap *out);
```

#### type_atlas_memory {#type_atlas_memory}

Source: `Type.h:352`

```c
size_t type_atlas_memory(uint32_t width, uint32_t height);
```

---------------------------------------------------------------------------
Shelf atlas. One byte per pixel, or one u32 (straight RGBA8) for color.
Storage is width*height samples inside the same buffer as the header.
-------------------------------------------------------------------------

#### type_atlas_rgba_memory {#type_atlas_rgba_memory}

Source: `Type.h:353`

```c
size_t type_atlas_rgba_memory(uint32_t width, uint32_t height);
```

#### type_atlas_place {#type_atlas_place}

Source: `Type.h:355`

```c
TypeAtlas *type_atlas_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);
```

#### type_atlas_rgba_place {#type_atlas_rgba_place}

Source: `Type.h:356`

```c
TypeAtlasRgba *type_atlas_rgba_place(void *buf, size_t bufsize, uint32_t width, uint32_t height);
```

#### type_atlas_pixels {#type_atlas_pixels}

Source: `Type.h:358`

```c
const uint8_t *type_atlas_pixels(const TypeAtlas *atlas, uint32_t *width, uint32_t *height);
```

#### type_atlas_rgba_pixels {#type_atlas_rgba_pixels}

Source: `Type.h:359`

```c
const uint32_t *type_atlas_rgba_pixels(const TypeAtlasRgba *atlas, uint32_t *width,
				       uint32_t *height);
```

#### type_atlas_pack {#type_atlas_pack}

Source: `Type.h:362`

```c
int type_atlas_pack(TypeAtlas *atlas, uint32_t w, uint32_t h,
		    TypeRect *out);
```

1 placed, 0 does not fit

#### type_atlas_rgba_pack {#type_atlas_rgba_pack}

Source: `Type.h:364`

```c
int type_atlas_rgba_pack(TypeAtlasRgba *atlas, uint32_t w, uint32_t h, TypeRect *out);
```

#### type_atlas_blit {#type_atlas_blit}

Source: `Type.h:365`

```c
void type_atlas_blit(TypeAtlas *atlas, const TypeRect *rect,
		     const uint8_t *src);
```

src is w*h coverage

#### type_atlas_rgba_blit {#type_atlas_rgba_blit}

Source: `Type.h:367`

```c
void type_atlas_rgba_blit(TypeAtlasRgba *atlas, const TypeRect *rect,
			  const uint8_t *src);
```

src is w*h*4

#### type_atlas_clear {#type_atlas_clear}

Source: `Type.h:369`

```c
void type_atlas_clear(TypeAtlas *atlas);
```

#### type_atlas_rgba_clear {#type_atlas_rgba_clear}

Source: `Type.h:370`

```c
void type_atlas_rgba_clear(TypeAtlasRgba *atlas);
```

#### type_memory {#type_memory}

Source: `Type.h:388`

```c
size_t type_memory(const TypeParams *params);
```

Context reserves memory for all our needs,
based on the parameters we pass to it.

NOTE(vasco): First added face becomes primary. A bold+italic face is remembered
as bold-italic; a bold face as bold; an italic face as italic. Later adds
of the same kind replace that slot. Fallback order is the set_fallbacks list.

Resolve: color emoji prefers a color fallback, then the style face
(bold-italic, else italic, else bold), then primary, then fallbacks.
A miss draws the primary .notdef (glyph id 0).

Printable ASCII (32..126) and U+FFFD at the current pixel size bypass the
LRU. Styled lookups always use the LRU. Changing size drops those slots
and warms ASCII again.
0 when params are unusable.

#### type_place {#type_place}

Source: `Type.h:389`

```c
TypeCtx *type_place(void *buf, size_t bufsize,
		    const TypeParams *params);
```

buf must be 16-byte aligned.

#### type_add_font {#type_add_font}

Source: `Type.h:393`

```c
TypeError type_add_font(TypeCtx *ctx, const void *bytes, size_t len, const TypeFaceParams *face,
			uint32_t *out_id);
```

Imported and bundled bytes receive identical bounded validation.

#### type_add_font_trusted {#type_add_font_trusted}

Source: `Type.h:395`

```c
TypeError type_add_font_trusted(TypeCtx *ctx, const void *bytes, size_t len,
				const TypeFaceParams *face, uint32_t *out_id);
```

#### type_clear_fonts {#type_clear_fonts}

Source: `Type.h:397`

```c
void type_clear_fonts(TypeCtx *ctx);
```

Drops faces. Font bytes may be freed after this returns.

#### type_select {#type_select}

Source: `Type.h:398`

```c
void type_select(TypeCtx *ctx, uint32_t font_id);
```

#### type_set_fallbacks {#type_set_fallbacks}

Source: `Type.h:399`

```c
TypeError type_set_fallbacks(TypeCtx *ctx, const uint32_t *ids, uint32_t count);
```

#### type_ctx_atlas_revision {#type_ctx_atlas_revision}

Source: `Type.h:403`

```c
uint64_t type_ctx_atlas_revision(const TypeCtx *ctx);
```

Coverage pixel revision; cache hits/packing alone do not change it.
Non-consuming: each GPU consumer keeps its own last uploaded revision.

#### type_ctx_atlas {#type_ctx_atlas}

Source: `Type.h:404`

```c
const uint8_t *type_ctx_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);
```

#### type_ctx_color_atlas {#type_ctx_color_atlas}

Source: `Type.h:405`

```c
const uint32_t *type_ctx_color_atlas(const TypeCtx *ctx, uint32_t *width, uint32_t *height);
```

#### type_metrics {#type_metrics}

Source: `Type.h:407`

```c
TypeError type_metrics(const TypeCtx *ctx, float size_px, TypeMetrics *out);
```

#### type_peek_glyph {#type_peek_glyph}

Source: `Type.h:411`

```c
TypeError type_peek_glyph(const TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
```

Cache probe only. Does not rasterize, does not move the LRU, does not bump stats.
TYPE_ERR_GLYPH_NOT_FOUND when the size does not match the ASCII cache or the key is cold.

#### type_peek_glyph_styled {#type_peek_glyph_styled}

Source: `Type.h:412`

```c
TypeError type_peek_glyph_styled(const TypeCtx *ctx, uint32_t codepoint, float size_px,
				 TypeStyle style, TypeGlyph *out);
```

#### type_glyph {#type_glyph}

Source: `Type.h:414`

```c
TypeError type_glyph(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeGlyph *out);
```

#### type_glyph_styled {#type_glyph_styled}

Source: `Type.h:415`

```c
TypeError type_glyph_styled(TypeCtx *ctx, uint32_t codepoint, float size_px, TypeStyle style,
			    TypeGlyph *out);
```

#### type_warm_ascii {#type_warm_ascii}

Source: `Type.h:417`

```c
TypeError type_warm_ascii(TypeCtx *ctx, float size_px);
```

#### type_glyph_id {#type_glyph_id}

Source: `Type.h:420`

```c
TypeError type_glyph_id(TypeCtx *ctx, uint32_t font_id, uint32_t glyph_id, float size_px,
			TypeGlyph *out);
```

Raster/cache by font-local ID, with no codepoint remapping or ASCII warmup.

#### type_layout_shaped {#type_layout_shaped}

Source: `Type.h:427`

```c
TypeError type_layout_shaped(TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
			     float size_px, TypePositionedGlyph *out, uint32_t cap,
			     uint32_t *written, TypeTextMetrics *metrics);
```

No shaping, bidi, wrapping or fallback is invented here. Use the actual
shaper's glyph order, IDs, clusters and positions. On error, written and
metrics describe the completed prefix; atlas/cache may retain that prefix.
metrics can be requested with out == NULL (cap still bounds the operation).
Glyph advance in out remains nominal; metrics uses shaped advances.

#### type_measure_shaped {#type_measure_shaped}

Source: `Type.h:432`

```c
TypeError type_measure_shaped(const TypeCtx *ctx, const TypeShapedGlyph *run, uint32_t count,
			      float size_px, uint32_t *written, TypeTextMetrics *metrics);
```

Same ink/advance computation without rasterization, scratch, atlas mutation
or cache statistics. count itself is the caller's work bound.

#### type_clear_atlas {#type_clear_atlas}

Source: `Type.h:436`

```c
void type_clear_atlas(TypeCtx *ctx);
```

Clear both atlases and the LRU, then warm ASCII at the last size.

#### type_layout_utf8 {#type_layout_utf8}

Source: `Type.h:442`

```c
TypeError type_layout_utf8(TypeCtx *ctx, const char *text, size_t len, float size_px, TypeCell *out,
			   uint32_t cap, uint32_t *written, size_t *bytes_used);
```

One line at baseline y = 0. x advances by glyph.advance. No wrap, no bidi.
On TYPE_ERR_BUF_TOO_SMALL, *written is cap and *bytes_used is the next
unconsumed byte. On TYPE_ERR_INVALID_UTF8, *bytes_used is the bad byte and
cells before it are kept. Both out-counts may be NULL.

#### type_stats {#type_stats}

Source: `Type.h:445`

```c
void type_stats(const TypeCtx *ctx, TypeGlyphStats *out);
```

#### type_utf8_next {#type_utf8_next}

Source: `Type.h:452`

```c
uint32_t type_utf8_next(const char *s, size_t len, uint32_t *codepoint);
```

---------------------------------------------------------------------------
UTF-8. type_utf8_next returns the byte length, or 0 when the sequence is
short or illegal (*codepoint is unchanged).
-------------------------------------------------------------------------

#### type_utf8_validate {#type_utf8_validate}

Source: `Type.h:453`

```c
int type_utf8_validate(const char *s, size_t len);
```

1 valid, 0 not

#### type_width {#type_width}

Source: `Type.h:459`

```c
TypeWidth type_width(uint32_t codepoint);
```

---------------------------------------------------------------------------
East Asian width and emoji class. cell width is 1 or 2.
-------------------------------------------------------------------------

#### type_is_cjk {#type_is_cjk}

Source: `Type.h:460`

```c
int type_is_cjk(uint32_t codepoint);
```

#### type_is_emoji {#type_is_emoji}

Source: `Type.h:461`

```c
int type_is_emoji(uint32_t codepoint);
```

#### type_cell_width {#type_cell_width}

Source: `Type.h:462`

```c
int type_cell_width(uint32_t codepoint);
```

#### type_is_combining {#type_is_combining}

Source: `Type.h:463`

```c
int type_is_combining(uint32_t codepoint);
```

<!-- END GENERATED API -->
