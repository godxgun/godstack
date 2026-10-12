# Type

Documentation: https://godxgun.github.io/godstack/docs/type/
Description: Place a Type context, load font bytes, and lay out and rasterize glyphs into a coverage atlas.


# Type

Type turns font files and UTF-8 text into positioned glyphs. It parses TrueType and OpenType
fonts (`glyf`, CFF and CFF2 outlines, and font collections), rasterizes glyphs with its own
antialiased rasterizer, and packs them into a coverage atlas that your renderer uploads as a
texture. Each glyph you get back says where it is in the atlas and where to draw it.

Type does not read files and does not allocate. Its state lives in a buffer you provide, font
bytes are borrowed from you, and temporary work uses a scratch buffer you attach. Font data is
validated with bounded reads, so a malformed or hostile font is rejected with an error instead of
being trusted.

Define `TYPE_IMPLEMENTATION` in exactly one translation unit before including `Type.h`, and link
with `-lm`.

## Creating a Context

The first thing to try is placing a context. The parameters fix the atlas size, the glyph cache
and how many faces the context can hold:

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define TYPE_IMPLEMENTATION
#include "Type.h"

int main(void)
{
    TypeParams params;
    type_params_default(&params);
    size_t bytes = type_memory(&params);
    if (!bytes || bytes > SIZE_MAX - 15) return 1;

    void *memory = malloc(bytes + 15);
    if (!memory) return 1;
    void *aligned = (void *)(((uintptr_t)memory + 15) & ~(uintptr_t)15);

    TypeCtx *ctx = type_place(aligned, bytes, &params);
    if (!ctx) {
        free(memory);
        return 1;
    }
    printf("%ux%u atlas, %u cached glyphs, up to %u faces: %zu bytes\n",
           params.atlas_width, params.atlas_height, params.cache_capacity,
           params.max_faces, bytes);
    free(memory);
    return 0;
}
```

`type_params_default()` fills in a 1024x1024 atlas, a 2048-glyph cache, eight faces and eight
fallbacks with box-filter antialiasing. `type_memory()` returns the bytes those parameters need,
or zero if they are unusable, and `type_place()` builds the context in your buffer. The buffer
must be 16-byte aligned, which is why the example rounds the pointer up. Nothing in the context
grows later; to change a limit, place a new context. There is no destroy call: free the buffer
when you are done.

## Rendering Text

With a context in place, add a font, attach scratch memory, and lay out a line. This program
loads the font named on the command line and prints a string as ASCII art:

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TYPE_IMPLEMENTATION
#include "Type.h"

enum { COLS = 100, ROWS = 24 };

static unsigned char font_bytes[16 << 20];
static unsigned char scratch_bytes[1 << 20];
static char picture[ROWS][COLS + 1];

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s font.ttf\n", argv[0]);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 1;
    size_t font_len = fread(font_bytes, 1, sizeof font_bytes, file);
    fclose(file);

    TypeParams params;
    type_params_default(&params);
    size_t bytes = type_memory(&params);
    void *memory = bytes ? malloc(bytes + 15) : NULL;
    if (!memory) return 1;
    TypeCtx *ctx = type_place((void *)(((uintptr_t)memory + 15) & ~(uintptr_t)15), bytes, &params);

    uint32_t font_id;
    TypeError error = ctx ? type_add_font(ctx, font_bytes, font_len, NULL, &font_id)
                          : TYPE_ERR_BUF_TOO_SMALL;
    if (error != TYPE_OK) {
        fprintf(stderr, "cannot use %s: error %d\n", argv[1], (int)error);
        free(memory);
        return 1;
    }

    TypeScratch scratch;
    type_scratch_init(&scratch, scratch_bytes, sizeof scratch_bytes);
    type_set_scratch(ctx, &scratch);

    const char text[] = "Hello, Type!";
    const float size = 20.0f;
    TypeCell cells[64];
    uint32_t count = 0;
    TypeMetrics metrics;
    error = type_layout_utf8(ctx, text, sizeof text - 1, size, cells, 64, &count, NULL);
    if (error == TYPE_OK) error = type_metrics(ctx, size, &metrics);
    type_set_scratch(ctx, NULL);
    if (error != TYPE_OK) {
        free(memory);
        return 1;
    }

    uint32_t atlas_w, atlas_h;
    const uint8_t *atlas = type_ctx_atlas(ctx, &atlas_w, &atlas_h);
    int baseline = (int)(metrics.ascender + 0.5f);
    memset(picture, ' ', sizeof picture);
    for (uint32_t i = 0; i < count; ++i) {
        const TypeGlyph *g = &cells[i].glyph;
        int left = (int)(cells[i].x + g->bearing_x);
        int top = baseline + (int)cells[i].y - (int)g->bearing_y;
        for (int y = 0; y < g->height; ++y)
            for (int x = 0; x < g->width; ++x) {
                uint8_t coverage = atlas[(size_t)(g->atlas_y + y) * atlas_w + g->atlas_x + x];
                int px = left + x, py = top + y;
                if (coverage > 64 && px >= 0 && px < COLS && py >= 0 && py < ROWS)
                    picture[py][px] = coverage > 160 ? '#' : '+';
            }
    }
    for (int row = 0; row < ROWS; ++row) {
        picture[row][COLS] = '\0';
        puts(picture[row]);
    }
    free(memory);
    return 0;
}
```

Build it with `cc -std=c99 -Wall -Wextra -I. hello_type.c -lm -o hello_type` and run
`./hello_type path/to/font.ttf`.

`type_add_font()` validates the font and returns an ID. Type keeps pointers into `font_bytes`, so
those bytes must stay alive and unchanged until `type_clear_fonts()` or until the context is
discarded. The first font added becomes the primary face. For a collection (`.ttc`), the first
face is used.

`type_layout_utf8()` lays out one line starting at the origin, with the baseline at `y = 0`. Each
`TypeCell` holds the codepoint, the pen position `x`, `y`, and a `TypeGlyph`. A glyph's image is
the rectangle `atlas_x`, `atlas_y`, `width`, `height` in the coverage atlas, and is drawn with its
left edge at `x + bearing_x` and its top edge at `y - bearing_y` (bearings point up from the
baseline). `type_metrics()` gives the primary face's ascender, descender and line gap at a pixel
size, for placing the baseline and spacing lines; the descender is negative, so the distance
between baselines is `ascender - descender + line_gap`. Sizes are rounded to whole pixels.

## Scratch Memory

Rasterizing a glyph that is not yet in the atlas needs temporary memory. `type_scratch_init()`
wraps a buffer you own, and `type_set_scratch()` lends it to the context until you pass NULL.
Without scratch, uncached glyphs fail with `TYPE_ERR_SCRATCH_REQUIRED`; if the buffer is too
small they fail with `TYPE_ERR_SCRATCH_FULL`.

Every call releases what it took from scratch before returning, so the buffer can be a per-frame
arena. Detach it before you reset or reuse that memory. `scratch.peak` records the most a call has
needed, which is a good guide for sizing. Cached glyphs need no scratch at all.

## Uploading the Atlas

Glyphs are packed into the atlas as they are first requested. The atlas is one byte of coverage
per pixel, `atlas_w * atlas_h` bytes, row by row:

```c
static uint64_t uploaded_revision;

uint32_t w, h;
const uint8_t *pixels = type_ctx_atlas(ctx, &w, &h);
uint64_t revision = type_ctx_atlas_revision(ctx);
if (revision != uploaded_revision) {
    upload_texture_r8(texture, pixels, w, h);   /* your renderer */
    uploaded_revision = revision;
}
```

The revision changes only when atlas pixels change, so cache hits cost no upload. Each texture
that mirrors the atlas tracks its own revision.

Printable ASCII and U+FFFD are kept in a fast table for the current size; the first request at a
new size rasterizes that whole set. Other glyphs live in an LRU cache of `cache_capacity`
entries. When the atlas has no room left, requests fail with `TYPE_ERR_ATLAS_FULL`;
`type_clear_atlas()` empties the atlas and cache so you can start over.

## Faces, Styles and Fallbacks

A context can hold several faces. Pass `TypeFaceParams` to mark a face as the bold, italic or
bold-italic member of the family, and list faces to try when the primary face has no glyph for a
codepoint:

```c
uint32_t regular, bold, symbols;
TypeFaceParams bold_face;
type_face_params_default(&bold_face);
bold_face.style.bold = 1;
bold_face.weight = TYPE_WEIGHT_BOLD;

if (type_add_font(ctx, regular_bytes, regular_len, NULL, &regular) != TYPE_OK) return 1;
if (type_add_font(ctx, bold_bytes, bold_len, &bold_face, &bold) != TYPE_OK) return 1;
if (type_add_font(ctx, symbol_bytes, symbol_len, NULL, &symbols) != TYPE_OK) return 1;
if (type_set_fallbacks(ctx, &symbols, 1) != TYPE_OK) return 1;

TypeStyle strong = {.italic = 0, .bold = 1};
TypeGlyph glyph;
TypeError error = type_glyph_styled(ctx, 'A', 18.0f, strong, &glyph);
```

`type_glyph()` and `type_glyph_styled()` return one glyph, rasterizing it if needed. A styled
request uses the matching style face, then the primary face, then the fallbacks in order. A
codepoint no face has is drawn with the primary face's `.notdef` glyph (glyph ID 0).
`type_select()` changes the primary face, and `type_peek_glyph()` checks the cache without
rasterizing.

## Shaped Text

`type_layout_utf8()` maps each codepoint to one glyph. It does not form ligatures, reorder
right-to-left text, place combining marks or wrap lines. For complex scripts, run a shaper such as
HarfBuzz first and pass its output to Type as `TypeShapedGlyph` records:

```c
TypeShapedGlyph run[] = {
    /* font_id, glyph_id, cluster, x_advance, y_advance, x_offset, y_offset */
    {regular, 73, 0, 11.0f, 0.0f, 0.0f, 0.0f},
    {regular, 89, 2, 9.5f, 0.0f, 0.0f, 0.0f},
};
TypePositionedGlyph placed[2];
uint32_t written = 0;
TypeTextMetrics ink;

if (type_layout_shaped(ctx, run, 2, 20.0f, placed, 2, &written, &ink) != TYPE_OK) return 1;
/* placed[i].x and placed[i].y are pen positions; ink.advance_x is the run width */
```

Glyph IDs, clusters (byte offsets into your text) and fractional advances come straight from the
shaper, in visual order and with y pointing down. `type_measure_shaped()` computes the same
advance and ink bounds without rasterizing anything, which suits line breaking. The
[`demos/type/`](https://github.com/godxgun/godstack/tree/main/demos/type) fixtures include a
HarfBuzz and FriBidi bridge.

## Lower-Level Building Blocks

The context is built from parts you can also use directly. `type_ttf_place()` parses font bytes
into a buffer of `type_ttf_memory()` bytes, and `type_font_from_ttf()` makes a face from it. A
face answers metric and glyph questions and renders single glyphs into your own memory:

```c
uint16_t glyph_id;
TypeBitmap info;
if (type_font_glyph_index(font, 'g', &glyph_id) != TYPE_OK) return 1;
if (type_font_bitmap_info(font, glyph_id, 32.0f, &info) != TYPE_OK) return 1;

size_t size = type_bitmap_bytes(&info);
uint8_t *coverage = malloc(size ? size : 1);
if (!coverage) return 1;
TypeError error = type_font_render_bitmap_scratch(font, glyph_id, 32.0f, TYPE_AA_BOX,
                                                  coverage, size, &scratch, &info);
```

`type_font_outline()` returns the glyph outline as move, line, quadratic and cubic segments in
font units, for tessellation or vector export. The atlas packer is available on its own as well:
`type_atlas_place()`, `type_atlas_pack()` and `type_atlas_blit()` shelf-pack any coverage images
into a buffer you provide.

`type_utf8_next()` decodes one codepoint and returns its byte length, or zero for an invalid or
truncated sequence; `type_utf8_validate()` checks a whole string.

Color emoji are not rendered, `TYPE_AA_SDF` returns `TYPE_ERR_UNIMPLEMENTED`, and the character
width and emoji queries (`type_width()`, `type_is_emoji()` and related calls) are placeholders.

See [Type symbols](/godstack/symbols/type/) for the generated declarations and their full contracts.
