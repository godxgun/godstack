/* ===========================================================================
 * FUSE - Immediate-mode UI command buffer - Copyright (c) 2025-2026 Vasco Alves
 * See LICENSE file for license info.
 *
 * DESCRITION:
 * UI layouting library for graphical applications.
 * Designed to be used to layout UI dynamically and easily.
 * There is a fallback bitmap font for convenience.
 *
 * FuseClass allows you to package a certain layouting configuration
 * into a struct similar to a class in CSS.
 *
 * Named tags allow you to get the names of specific elements so you can 
 * specific operations on them.
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
 * PREFIX: 
 *     FUSE_ (macros)  Fuse (types)  fuse_ (functions)
 *
 * TODO:
 * - Built-in tabs (?) Not that hard to do.
 *
 * =========================================================================== */

#ifndef FUSE_H
#define FUSE_H

#define FUSE_MAJOR 0
#define FUSE_MINOR 10
#define FUSE_PATCH 7

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
 * 0.10.2 - @vasco - integer pixel grid: boxes, commands, 5x7 scale
 * 0.10.3 - @vasco - debug inspector: element boxes and a tree panel
 * 0.10.4 - @vasco - columns; 5x7 text is one layout element
 * 0.10.5 - @vasco - image element: a box plus a caller handle
 * 0.10.6 - @vasco - per-child sizing; space; button label is a child
 * 0.10.7 - @vasco - text box: focus, edit queue, clipped text command
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


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
    FUSE_ERR_BUF_TOO_SMALL,   /* bufsize < fuse_canvas_memory(n) for any n, or unaligned memory */
    FUSE_ERR_OVERFLOW,        /* a widget/cmd would exceed max_elements or cmd capacity */
    FUSE_ERR_DUPLICATE_ID,    /* same id emitted twice in one generation */
    FUSE_ERR_UNBALANCED,      /* div_end with empty stack, or draw with open divs */
} FuseError;

typedef enum {
    FUSE_CMD_RECT = 0,
    FUSE_CMD_LINE,
    FUSE_CMD_CLIP_START,
    FUSE_CMD_CLIP_END,
    FUSE_CMD_IMAGE,
    FUSE_CMD_TEXT,
    FUSE_CMD_COUNT,
} FuseCmdType;

typedef struct {
    float x, y, w, h;
    uint32_t color;
} FuseCmdRect;

typedef struct {
    float x1, y1, x2, y2;
    float thickness;
    uint32_t color;
} FuseCmdLine;

typedef struct {
    float x, y, w, h;
} FuseCmdClip;

/**
 * Laid-out image box, like an img with a width and height. x,y,w,h are
 * canvas pixels and are not snapped. Handle is the caller's.
 * Fuse does not store pixels. 
 */
typedef struct {
    float x, y, w, h;
    uint32_t handle;
} FuseCmdImage;

/* Proportional text. The command is only the clip box plus a slot.
 * Glyphs, caret, and the string live in FuseFieldRun so this union
 * stays the size of a line command. */
typedef struct {
    float x, y, w, h;
    uint32_t slot;
} FuseCmdText;

/* One text box for this frame. str stays valid through draw.
 * caret is a byte index, or -1 when hidden. */
typedef struct FuseFieldRun {
    const char *str;
    float size;
    int32_t caret;
    uint32_t color;
    uint32_t caret_color;
} FuseFieldRun;

/**
 * FuseCmd is the basic unit emited by fuse that your renderer will turn
 * into One draw record. draw() returns these packed, already in canvas space,
 * z-stable in emit order (z is reserved, currently 0). id is the widget
 * that produced the command, or 0 for anonymous geometry.
 */
typedef struct {
    union {
        FuseCmdRect rect;
        FuseCmdLine line;
        FuseCmdClip clip;
        FuseCmdImage image;
        FuseCmdText text;
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
// Reusable style passed down to elements and their children (in the case of divs).
typedef struct FuseClass {
    uint32_t color;                 /* 0 = no RECT */
    float    width, height;         /* FIXED px or PERCENT 0..1, for children */
    float    min_width, min_height;
    float    max_width, max_height;
    float    gap;
    float    pad_l, pad_r, pad_t, pad_b;

    /* NOTE(vasco): Hmm, I wonder if I can merge
     * all of these into one flag. */
    uint8_t  direction;             /* FuseDirection */
    uint8_t  width_sizing;          /* FuseSizing */
    uint8_t  height_sizing;         /* FuseSizing */
    uint8_t  align_x;               /* FuseAlign */
    uint8_t  align_y;               /* FuseAlign */
} FuseClass;


// ███ ███ ███ █ █ ███ ███
// █     █ █ █ █ █   █ █
// █   ███ █ █ █ █ ███   █
// ███ ███ █ █  █  ███ ███
// 
// The canvas is the "context" of Fuse UI and it represents
// where elements are "drawn" to. 
//
// +------------------------------------------------+ 
// | [Button]                                       |
// | Text                                           |
// | 0.5 |---o---|                                  |
// |                                                |
// |                                                |
// +------------------------------------------------+ 

typedef struct fuse_canvas_t *FuseCanvas; // Opaque

size_t     fuse_canvas_memory(size_t max_elements); // Returns amount of memory required.
FuseCanvas fuse_canvas_create(void *buf, size_t bufsize); // Creates the canvas and places it in the allocated memory region.
void       fuse_canvas_clear(FuseCanvas); 
void       fuse_canvas_resize(FuseCanvas, float w, float h);
void       fuse_canvas_pointer(FuseCanvas, FusePointerState pointer_state, float x, float y);
void       fuse_canvas_wheel(FuseCanvas, float dx, float dy); // +dy is wheel up

/* Editing keys for the focused text box. Queued until that box is built. */
typedef enum {
    FUSE_KEY_BACKSPACE = 1,
    FUSE_KEY_DELETE,
    FUSE_KEY_LEFT,
    FUSE_KEY_RIGHT,
    FUSE_KEY_HOME,
    FUSE_KEY_END,
} FuseKey;

void       fuse_canvas_text(FuseCanvas, const char *utf8);
void       fuse_canvas_key(FuseCanvas, FuseKey key);
/* NULL clears. The name is the fuse_id of a text box. */
void       fuse_focus(FuseCanvas, const char *name);
bool       fuse_focused(FuseCanvas, const char *name);

FuseCmd   *fuse_canvas_draw(FuseCanvas, size_t *cmd_count);
/* Valid until the next clear. Parallel to FUSE_CMD_TEXT slots. */
const FuseFieldRun *fuse_canvas_fields(FuseCanvas, size_t *count);

void       fuse_canvas_debug(FuseCanvas, bool enabled);

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

/* Divs are fixed rectangles that act like mini canvases. 
 * You could technachly do anything by using just divs 
 * and passing the appropiate parameters.
 * +------------------------------------------------+ 
 * |    col1 |                                 col2 |
 * |         |                 _______              |
 * |_________|                |   div |             |
 * |     div |                |_______|             |
 * |         |                                      |
 * |         |                                      |
 * +------------------------------------------------+ 
 * Columns allow you to split up space horizontally according
 * to specific ratios like [0.3, 0.7] for example.
 */

void fuse_div_begin(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls);
void fuse_div_begin_scroll(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls, float *scroll);
void fuse_div_end(FuseCanvas);
void fuse_col_begin(FuseCanvas, float x, float y, float w, const FuseClass *cls, const float *ratios, uint32_t n);
void fuse_col_switch(FuseCanvas);
void fuse_col_end(FuseCanvas);

/* Windows / docking is a necessary feature of modern editing software.
 * Maybe in a 2.0 version of Fuse, I could find a way to merge divs
 * and windows, but I don't find that necessary or an issue.
 *
 * CANVAS
 * +-----------------------+------------------------+ 
 * |  _______           DIV|            |    DOCKING|
 * | |   _[]X|     _______ |            |           |
 * | |       |    |   _[]X||           <|>          |
 * | |       |    |       ||           <|>          |
 * | |_______|    |       ||            |           |    
 * |              |_______||win1        |win2       |                
 * +------------------------------------------------+ 
 *
 * Windows act similar to divs in the sense that they also act as 
 * containers for the elements drawn inside of them.
 *
 * A docking div would allow you to customize how you want windows to
 * be docked / tiled. Otherwise, windows will act in a "floating" manner
 * similar to what is seen in non-tiling desktop environments like Windows.
 */

void fuse_window_begin(FuseCanvas, char *name, float x, float y, float w, float h, const FuseClass *cls);
void fuse_window_begin_scroll(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls, float *scroll);
void fuse_window_end(FuseCanvas);

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

/* Stems the next element. Replaces the parent class on that axis.
 * FIT keeps the size passed in. GROW takes leftover. FIXED and PERCENT
 * use the size passed in (pixels, or 0..1). Omit the call to inherit. */
void fuse_sizing(FuseCanvas, uint8_t width_sizing, uint8_t height_sizing);
/* Empty child. Grows on the parent main axis. Needs a classed parent. */
void fuse_space(FuseCanvas);

/* Clicked this frame: last-frame hover + released-this-frame.
 * selected is the idle fill, hovered the hover/press fill. Local space. */
bool fuse_button(FuseCanvas, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);

/* Button plus centered 5x7 label. The label is a child, recentered from
 * the final box. Ink is 0xFF000000. */
bool fuse_button_text(FuseCanvas, char *text, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);
void fuse_slider(FuseCanvas, float x, float y, float w, float h, uint32_t track, uint32_t nob, float *nob_pos);

/* One-line field. Call fuse_id first. buf is NUL-terminated, cap includes the NUL.
 * pad_l and pad_r inset the glyphs. placeholder is drawn when buf is empty.
 * caret_on shows the caret while the box is focused. allow may be NULL,
 * which keeps bytes 32..126. Returns 1 when buf changes. */
bool fuse_textbox(FuseCanvas, float x, float y, float w, float h, float pad_l, float pad_r,
    char *buf, int cap, int *caret, const char *placeholder, float size,
    uint32_t fill, uint32_t hovered, uint32_t ink, uint32_t ghost, int caret_on,
    int (*allow)(unsigned char ch, void *user), void *user);
/* Applies the queued edit to buf when name is the focused field.
 * Use this before layout that depends on the text. The later text box
 * call draws that result and does not apply the same keys again. */
bool fuse_textbox_edit(FuseCanvas, const char *name, char *buf, int cap, int *caret,
    int (*allow)(unsigned char ch, void *user), void *user);

//   █
//   █
// ███ ███ ███ █ █ █
// █ █ █     █ █ █ █
// █ █ █   ███ █ █ █
// ███ █   ███  █ █
//
void    fuse_rect(FuseCanvas, float x, float y, float w, float h, uint32_t color);

/* Image in the layout, same coordinates as fuse_rect. w and h are the
 * displayed size (intrinsic pixels times the caller's scale). */
void    fuse_image(FuseCanvas, float x, float y, float w, float h, uint32_t handle);

//  █           █
// ███ ███ █ █ ███
//  █  ███  █   █
//  █  █    █   █
//  ██ ███ █ █  ██
//
//  Text, font and bitmaps.

/* Built-in 5x7 bitmap, the fallback face. One layout element per string;
 * glyph runs are its children, each glyph pixel s by s. s snaps to an integer. */
void    fuse_text(FuseCanvas, float x, float y, float s, const char *str, uint32_t color);
void    fuse_text_utf8(FuseCanvas, float x, float y, float s, const uint32_t *str, uint32_t color);
float   fuse_text_width(const char *str, float s);

#endif /* FUSE_H */
