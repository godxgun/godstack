/* ===========================================================================
 * FUSE - Immediate-mode UI command buffer - Copyright (c) 2025-2026 Vasco Alves
 * See LICENSE file for license info.
 *
 * DESCRITION:
 * Mainly a UI layouting library for games. Designed to be used to layout
 * game UI dynamically and easily. There is a fallback bitmap font.
 *
 * Not everything is a flexbox like in Clay but flexboxes do exist.
 * Flexbox takes time to compute. Not that much but still not necessary.
 *
 * FuseClass allows you to package a certain layouting configuration
 * into a struct.
 *
 * Named tags allow you to get the names of specific elements.
 *
 * uint64_t ids allows the user to create custom widgets and plugins,
 * that can be identified by the 
 * 
 *
 * SHOUTOUTS:
 * - https://caseymuratori.com/blog_0001
 *   I've been writting my own UI code ever since I stumbled upon this
 *   blog post by Casey Muratori himself.
 * - https://github.com/immediate-mode-ui/nuklear
 *   Nuklear is the most complete single header UI library, but something
 *   about it feels bloated. Probably due to including several stb headers
 *   inlined into it.
 * - https://github.com/nicbarker/clay
 *   Clay was a big inspiration, especially architecture wise for FUSE.
 *   It was that simpler architecture I was looking for in a UI library
 *   without being completely stripped down.
 * - https://github.com/rxi/microui/tree/master
 *   Being only 1100 sloc 
 *
 *   PREFIX: 
 *     FUSE_ (macros)  Fuse (types)  fuse_ (functions)
 *
 * TODO:
 * - Tabs.
 *
 * =========================================================================== */

#ifndef FUSE_H
#define FUSE_H

#define FUSE_MAJOR 0
#define FUSE_MINOR 10
#define FUSE_PATCH 1

/* CHANGE LOG
 * 0.0.1 - @vasco - slider
 * 0.1.0 - @vasco - canvas memory / create; no malloc, no globals
 * 0.2.0 - @vasco - RECT LINE CLIP_START CLIP_END
 * 0.3.0 - @vasco - clear / draw; local to canvas
 * 0.4.0 - @vasco - screen stack; div_begin / div_end
 * 0.5.0 - @vasco - hash ids; last-frame boxes
 * 0.6.0 - @vasco - pointer edges; button / slider
 * 0.7.0 - @vasco - FuseClass child layout
 * 0.8.0 - @vasco - nested sidebar screen + button + slider
 * 0.9.0 - @vasco - rect, 5x7 text, button_text
 * 0.10.0 - @vasco - scrollable divs; wheel
 * 0.10.1 - @vasco - canvas memory: cap screens, pack elements
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct fuse_canvas_t *FuseCanvas;

typedef enum {
    FUSE_DIRECTION_ROW = 0,
    FUSE_DIRECTION_COLUMN,
} FuseDirection;

typedef enum {
    FUSE_POINTER_NONE = 0,
    FUSE_POINTER_RELEASED,
    FUSE_POINTER_PRESSED,
    FUSE_POINTER_ALT,
} FusePointerState;

typedef enum {
    FUSE_SIZING_FIT = 0,      /* shrink-wrap children */
    FUSE_SIZING_GROW,         /* take leftover space on that axis */
    FUSE_SIZING_FIXED,        /* width/height are pixels */
    FUSE_SIZING_PERCENT,      /* width/height are 0..1 of current screen minus padding/gaps */
} FuseSizing;

typedef enum {
    FUSE_ALIGN_START = 0,     /* left / top */
    FUSE_ALIGN_CENTER,
    FUSE_ALIGN_END,           /* right / bottom */
} FuseAlign;

typedef enum {
    FUSE_ERR_OK = 0,
    FUSE_ERR_BUF_TOO_SMALL,   /* create: bufsize < fuse_canvas_memory(n) for any n, or unaligned */
    FUSE_ERR_OVERFLOW,        /* a widget/cmd would exceed max_elements or cmd capacity */
    FUSE_ERR_DUPLICATE_ID,    /* same id emitted twice in one generation */
    FUSE_ERR_UNBALANCED,      /* div_end with empty stack, or draw with open divs */
} FuseError;

typedef enum {
    FUSE_CMD_RECT = 0,
    FUSE_CMD_LINE,
    FUSE_CMD_CLIP_START,
    FUSE_CMD_CLIP_END,
    FUSE_CMD_COUNT,
} FuseCmdType;

/* Axis-aligned filled rectangle in canvas pixels. */
typedef struct {
    float x, y, w, h;
    uint32_t color;
} FuseCmdRect;

/* Stroke from (x1,y1) to (x2,y2) in canvas pixels. */
typedef struct {
    float x1, y1, x2, y2;
    float thickness;
    uint32_t color;
} FuseCmdLine;

/* Clip rectangle in canvas pixels. CLIP_START pushes; CLIP_END pops. */
typedef struct {
    float x, y, w, h;
} FuseCmdClip;

/*
 * One draw record. draw() returns these packed, already in canvas space,
 * z-stable in emit order (z is reserved, currently 0). id is the widget
 * that produced the command, or 0 for anonymous geometry.
 */
typedef struct {
    union {
        FuseCmdRect rect;
        FuseCmdLine line;
        FuseCmdClip clip;
    };
    uint32_t id;
    int16_t  z;
    uint8_t  type;            /* FuseCmdType */
} FuseCmd;

//     █
//     █
// ███ █  ███ ███ ███
// █   █    █ █   █
// █   █  ███   █   █
// ███ ██ ███ ███ ███
//
// Reusable style passed down to elements and their children.
typedef struct FuseClass {
    uint32_t color;                 /* 0 = no RECT */
    uint8_t  direction;             /* FuseDirection */
    uint8_t  width_sizing;          /* FuseSizing */
    uint8_t  height_sizing;         /* FuseSizing */
    float    width, height;         /* FIXED px or PERCENT 0..1, for children */
    float    min_width, min_height;
    float    max_width, max_height;
    float    gap;
    float    pad_l, pad_r, pad_t, pad_b;
    uint8_t  align_x;               /* FuseAlign */
    uint8_t  align_y;               /* FuseAlign */
} FuseClass;


// ███ ███ ███ █ █ ███ ███
// █     █ █ █ █ █   █ █
// █   ███ █ █ █ █ ███   █
// ███ ███ █ █  █  ███ ███
// 
// The "Context" of Fuse UI where elements are "drawn" to.
//
size_t     fuse_canvas_memory(size_t max_elements); // Returns amount of memory required.
FuseCanvas fuse_canvas_create(void *buf, size_t bufsize); // Creates the canvas and places it in the allocated memory region.
FuseError  fuse_canvas_error(FuseCanvas);
void       fuse_canvas_clear(FuseCanvas);
void       fuse_canvas_resize(FuseCanvas, float w, float h);
void       fuse_canvas_pointer(FuseCanvas, FusePointerState pointer_state, float x, float y);
void       fuse_canvas_wheel(FuseCanvas, float dx, float dy); /* accumulate; +dy is wheel up */
FuseCmd   *fuse_canvas_draw(FuseCanvas, size_t *cmd_count);
// useful
float fuse_percent_x(FuseCanvas, float p);
float fuse_percent_y(FuseCanvas, float p);


// █                      █
// █                   █
// █  ███ █ █ ███ █ █ ███ █ ███ ███
// █    █ █ █ █ █ █ █  █  █ █ █ █ █
// █  ███ █ █ █ █ █ █  █  █ █ █ █ █
// ██ ███ ███ ███ ███  ██ █ █ █ ███
//          █                     █
//        ███                   ███
// 
void fuse_div_begin(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls);
void fuse_div_begin_scroll(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls, float *scroll);
void fuse_div_end(FuseCanvas);

//     █
//     █                     █
// ███ █  ███ █████ ███ ███ ███ ███
// ███ █  ███ █ █ █ ███ █ █  █  █
// █   █  █   █ █ █ █   █ █  █    █
// ███ ██ ███ █ █ █ ███ █ █  ██ ███
//
// id functions stem the next element with a name
// We can then extract information from that tag.
void fuse_id(FuseCanvas, const char *name);
void fuse_idi(FuseCanvas, const char *name, int index);
bool fuse_element_is_hovered(FuseCanvas, const char *name);

/* Clicked this frame: last-frame hover + released-this-frame.
 * selected is the idle fill, hovered the hover/press fill. Local space. */
bool fuse_button(FuseCanvas, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);

/* Button plus centered 5x7 label. Ink is 0xFF000000. */
bool fuse_button_text(FuseCanvas, char *text, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);
void fuse_slider(FuseCanvas, float x, float y, float w, float h, uint32_t track, uint32_t nob, float *nob_pos);

//   █
//   █
// ███ ███ ███ █ █ █
// █ █ █     █ █ █ █
// █ █ █   ███ █ █ █
// ███ █   ███  █ █
//
void    fuse_rect(FuseCanvas, float x, float y, float w, float h, uint32_t color);

//  █           █
// ███ ███ █ █ ███
//  █  ███  █   █
//  █  █    █   █
//  ██ ███ █ █  ██
//
//  Text, font and bitmaps.

// FuseFont
//
void    fuse_text(FuseCanvas, float x, float y, float s, const char *str, uint32_t color); // 5x7 bitmap, one RECT per run. Scale s is pixel size of one glyph pixel.
float   fuse_text_width(const char *str, float s); // Advance is 6*s per character, last gap omitted.



#endif /* FUSE_H */
