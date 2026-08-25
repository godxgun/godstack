/* ===========================================================================
 * FUSE - Immediate-mode UI command buffer - Copyright (c) 2025-2026 Vasco Alves
 * See LICENSE file for license info.
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
 *   1100 sloc 
 *
 * PREFIX: FUSE_ (macros)  Fuse (types)  fuse_ (functions)
 *
 * PLAN:
 *   Immediate-mode UI that emits a command buffer. The user renders those
 *   commands with whatever backend they want (Rend, software, etc.). Fuse
 *   does not draw pixels. Clay is the reference for layout, ids, and cmds.
 *   The public API is argument-heavy; shorthands come later if ever.
 *
 *   Constraints
 *     - No malloc inside Fuse. User supplies one buffer. fuse_canvas_memory(n)
 *       is the only size oracle. Overflow of max_elements is the only runtime
 *       failure besides unbalanced begin/end and duplicate ids.
 *     - No file-scope / thread-local current canvas. Every call takes
 *       FuseCanvas first. Two canvases on one thread are isolated.
 *     - No fonts, no text measurement, no text widgets in this plan.
 *       Fuse only compiles layout into commands. Pointer is the only input.
 *     - No public Rect type. Four floats are a box. Color is float rgba;
 *       Fuse does not pack channels. The backend interprets them.
 *     - No FuseStyle / FuseWindow / FuseFont objects.
 *     - Tweens (fuse_tween.c) are not part of this plan.
 *     - One container: div. Layout and fill live on a user-owned FuseClass
 *       passed into the div. NULL class = box only, no fill, children placed
 *       absolutely in that screen. A widget may override that class with
 *       fuse_class(id, class).
 *     - Commands live in this header. fuse_backend.h goes away.
 *     - Coordinates are pixels, not normalized. (0,0) is top-left of the
 *       *current screen*, not the root canvas, once a div is open.
 *     - Implementation TU is fuse.c, included via -I Fuse.
 *     - Each div emits CLIP_START / CLIP_END automatically. No public clip call.
 *     - Failed calls return a stub (false, NULL, or no emit) and set a sticky
 *       error. Later calls no-op until a successful clear(). BUF_TOO_SMALL is
 *       create-only (NULL canvas).
 *
 *   Architecture
 *     User owns buf. create() carves it into header + hashmap + element tree
 *     + screen stack + command array + scratch. Nothing else
 *     is allocated. draw() writes a sorted FuseCmd array back into that
 *     same buffer and returns a pointer valid until the next clear/draw.
 *
 *     Screen stack: fuse_div_begin pushes a smaller screen. (0,0) is that
 *     div's top-left; w,h are its size. Children speak that space.
 *     fuse_div_end pops. Root screen is the canvas (fuse_canvas_resize).
 *     draw() converts every command to canvas space by walking the tree.
 *     fuse_percent_x/y use the current screen, not the root.
 *
 *     Ids: fuse_id / fuse_idi stamp pending_id on the canvas. The next
 *     div/button/slider consumes it. No stamp → auto id hash(sibling, parent).
 *     id 0 is reserved. Same id + same generation = FUSE_ERR_DUPLICATE_ID.
 *     Hover names hash the same way as fuse_id.
 *
 *     Hit testing uses last-frame boxes stored in the hashmap, in canvas
 *     space. One frame late by design. Pointer edges live on the canvas:
 *     pressed-this-frame / pressed / released-this-frame / released.
 *     Button click = last-frame hover + released-this-frame. Slider captures
 *     id on the canvas until release.
 *
 *     draw() culls off-canvas RECT/LINE. Never cull CLIP_START / CLIP_END.
 *     Every div is wrapped in a clip pair.
 *
 *   Internal (not public API; lives in the user buffer)
 *     Screen stack is the open stack. Canvas-space origin is the tree walk at draw.
 *
 *       FuseScreen     { element }  open element; size is that element's w,h
 *       FuseElement    { id; x,y,w,h local; color; cls; first_child, child_count; flags }
 *       FuseHashItem   { id, generation; x,y,w,h canvas-space last frame }
 *       fuse_canvas_t  {
 *           max_elements;
 *           elements, element_count, cmd_count;
 *           screens, screen_count, cmds, hash;
 *           generation, capture_id, pending_id;
 *           width, height, pointer_x, pointer_y;
 *           pointer_state, pointer_edge;
 *       }
 *
 *   Frame
 *     resize, pointer, clear, widgets, draw, iterate cmds. Repeat.
 *
 *   Build order
 *     1. memory / create(buf, bufsize) / no malloc / no globals
 *     2. RECT LINE CLIP_START CLIP_END
 *     3. clear / draw → cmds + count, local → canvas
 *     4. screen stack (div_begin / div_end)
 *     5. hash ids + last-frame boxes (canvas space)
 *     6. pointer edges; button / slider in local space
 *     7. FuseClass child layout inside a screen
 *     8. test: nested sidebar screen + button + slider
 *
 *   Done when
 *     no Fuse malloc, no file-scope state, two canvases isolated,
 *     local (0,0) in a div is the div's origin, insert-before-button
 *     does not steal click.
 *
 *   Out of scope
 *     fonts, text, checkbox, tooltip, hover callbacks, rotate,
 *     flex_begin/end, windows, 3d, animate_*, tween_*, renderer
 *     callbacks, fuse_canvas_set / destroy / debug, globals,
 *     keyboard, scroll, multi-pointer, z API (cmd has a z field
 *     reserved; nothing public writes it yet).
 * =========================================================================== */

#ifndef FUSE_H
#define FUSE_H

#define FUSE_MAJOR 0
#define FUSE_MINOR 8
#define FUSE_PATCH 0

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

/*
 * Reusable style. User-owned, may be static. Passed to fuse_div_begin.
 * NULL means: push a screen of the given box, no background RECT, children
 * absolutely placed. Non-NULL: optional RECT if color != 0; children laid
 * out by direction/gap/padding/sizing *inside* that screen.
 *
 * width/height apply to *children* of this class (FIXED px or PERCENT 0..1),
 * not to the div's own screen — the div's screen is the x,y,w,h on begin.
 */
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

/* Bytes needed for a canvas that can hold max_elements widgets this frame.
 * Includes hashmap, tree, screen stack, cmds, scratch.
 * create() requires bufsize >= this value for the chosen n. */
size_t     fuse_canvas_memory(size_t max_elements);

/* Place a canvas over user memory. Does not allocate. Returns NULL if
 * buf is NULL or bufsize is too small. No destroy: drop the buffer. */
FuseCanvas fuse_canvas_create(void *buf, size_t bufsize);

/* Sticky error on this canvas. Stays until the next successful clear(),
 * except FUSE_ERR_BUF_TOO_SMALL which is create-only (NULL canvas). */
FuseError  fuse_canvas_error(FuseCanvas);

/* Reset the frame tree. Does not clear last-frame hit boxes or pointer
 * capture. Call once per frame before widgets. */
void       fuse_canvas_clear(FuseCanvas);

/* Root screen size in pixels. (0,0) of the empty stack is top-left. */
void       fuse_canvas_resize(FuseCanvas, float w, float h);

/* Pointer in canvas pixels. pointer_state is FusePointerState.
 * Edges (pressed-this-frame / released-this-frame) are derived here
 * from the previous state. */
void       fuse_canvas_pointer(FuseCanvas, int pointer_state, float x, float y);

/* Resolve layout, convert local boxes to canvas space, emit cmds.
 * Returns the command array inside buf, valid until next clear/draw.
 * Writes count. On error returns NULL and sets *cmd_count = 0. */
FuseCmd   *fuse_canvas_draw(FuseCanvas, size_t *cmd_count);

/*
 * Push a screen. x,y,w,h are in the *current* screen. w and h must be > 0.
 * cls == NULL → no fill, children absolutely placed.
 * cls != NULL → optional background RECT; children laid out by class.
 */
void fuse_div_begin(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls);
void fuse_div_end(FuseCanvas);

/* Stamp the next div/button/slider. Consumed by that next call.
 * fuse_idi is for lists (name, index). Hover queries use the same hash. */
void fuse_id(FuseCanvas, const char *name);
void fuse_idi(FuseCanvas, const char *name, int index);

/* True if the named element's last-frame box contains the pointer. */
bool fuse_element_is_hovered(FuseCanvas, const char *name);

/* Clicked this frame: last-frame hover + released-this-frame.
 * selected is the idle fill, hovered the hover/press fill. Local space. */
bool fuse_button(FuseCanvas, float x, float y, float w, float h,
                 uint32_t selected, uint32_t hovered);

/* Horizontal slider. nob_pos is 0..1, written while captured.
 * Capture holds until release even if the pointer leaves the track. */
void fuse_slider(FuseCanvas, float x, float y, float w, float h,
                 uint32_t track, uint32_t nob, float *nob_pos);

/* p percent of the *current* screen. Left/top is 0. */
float fuse_percent_x(FuseCanvas, float p);
float fuse_percent_y(FuseCanvas, float p);

#endif /* FUSE_H */
