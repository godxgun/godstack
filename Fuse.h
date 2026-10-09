/* ===========================================================================
 * FUSE - Immediate-mode UI command buffer - Copyright (c) 2025-2026 Vasco Alves
 * See LICENSE file for license info.
 *
 * ███ █ █ ███ ███
 * █   █ █ █   █
 * ███ █ █  █  ███
 * █   █ █   █ █
 * █   ███ ███ ███
 *
 * figlet font: maxiwi
 *
 * DESCRITION:
 * Fuse is a generic layouting library built for perfomance.
 * Fundamentally, it provides functions for drawing basic UI 
 * widgets and for 
 *
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
 * USAGE:
 *     #define FUSE_IMPLEMENTATION in exactly one translation unit
 *     #include "Fuse.h"
 *     Other translation units include "Fuse.h" without the macro.
 *     C99 or later; link with libm (-lm where required).
 *     Core and docking borrow caller memory and need only standard C headers.
 *     Optional Rend/Type adapter: include "FuseRend.h" separately and define
 *     FUSE_REND_IMPLEMENTATION in one translation unit for that adapter.
 *
 * PREFIX: 
 *     FUSE_ (macros)  Fuse (types)  fuse_ (functions)
 *
 * =========================================================================== */

#ifndef FUSE_H
#define FUSE_H

/* CHANGE LOG
 * 0.0.1 - slider
 * 0.1.0 - canvas memory / create; no malloc, no globals
 * 0.2.0 - RECT LINE CLIP_START CLIP_END
 * 0.3.0 - clear / draw; local to canvas
 * 0.4.0 - screen stack; div_begin / div_end
 * 0.5.0 - hash ids; last-frame boxes
 * 0.6.0 - pointer edges; button / slider
 * 0.7.0 - FuseClass child layout
 * 0.8.0 - nested sidebar screen + button + slider
 * 0.9.0 - rect, 5x7 text, button_text
 * 0.10.0 - scrollable divs; wheel
 * 0.10.1 - canvas memory: cap screens, pack elements
 * 0.10.2 - integer pixel grid: boxes, commands, 5x7 scale
 * 0.10.3 - debug inspector: element boxes and a tree panel
 * 0.10.4 - columns; 5x7 text is one layout element
 * 0.10.5 - image element: a box plus a caller handle
 * 0.10.7 - text box: focus, edit queue, clipped text command
 * 0.10.8 - radial drawing/picking and triangle commands; link with libm
 * 0.11.0 - caller-backed docking and explicit command layers; rebuild consumers
 * 0.12.0 - explicit FuseParams placement/capacity; failed builds cancel input
 * 0.13.0 - docking widgets take per-call colors; rebuild/update their callers
 */

#define FUSE_MAJOR 0
#define FUSE_MINOR 13
#define FUSE_PATCH 0

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
    FUSE_ERR_BUF_TOO_SMALL,   /* insufficient or unaligned backing */
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
    FUSE_CMD_TRIANGLE,
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

typedef struct {
    float x1, y1, x2, y2, x3, y3;
    uint32_t color;
} FuseCmdTriangle;

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
 * does not enlarge the command union. */
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
 * in emit order; z is the explicit caller layer (default 0). id is the widget
 * that produced the command, or 0 for anonymous geometry.
 */
typedef struct {
    union {
        FuseCmdRect rect;
        FuseCmdLine line;
        FuseCmdClip clip;
        FuseCmdImage image;
        FuseCmdText text;
        FuseCmdTriangle triangle;
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

/* Independent bounds: element capacity includes the implicit root; scopes
 * count total stack slots including that root (32 permits 31 explicit scopes).
 * max_windows is the dock registration bound, not another canvas registry.
 * Params are copied; backing is borrowed, 8-byte aligned, and never freed.
 * Sizing returns 0 and placement returns NULL for invalid/overflowing counts,
 * misalignment or short backing, before modifying caller storage. */
#define FUSE_WINDOWS_MAX 16
typedef struct FuseParams {
    size_t max_elements;
    size_t max_windows;
    size_t max_scopes;
} FuseParams;

size_t     fuse_memory(const FuseParams *params);
FuseCanvas fuse_place_in_memory(void *buf, size_t bytes, const FuseParams *params);
size_t     fuse_canvas_max_windows(FuseCanvas);
/* Highest total stack depth in this build, reset to the root (1) on clear. */
size_t     fuse_canvas_scope_peak(FuseCanvas);
FuseError  fuse_canvas_error(FuseCanvas);
/* Legacy helpers: 16 window registrations and min(max_elements,32) scopes.
 * create infers the greatest fitting element capacity; no allocation/fallback. */
size_t     fuse_canvas_memory(size_t max_elements);
FuseCanvas fuse_canvas_create(void *buf, size_t bufsize);
void       fuse_canvas_clear(FuseCanvas);
/* Applies to subsequently built elements. Resets to 0 on clear. */
void       fuse_canvas_layer(FuseCanvas, int16_t layer);
/* Gate pointer/wheel picking and captured slider updates for subsequent widgets.
 * Keyboard focus/edits survive; capture still ends on release. Resets on clear.
 * This is a builder setting, not a stack: callers restore it after a panel. */
void       fuse_canvas_input_enabled(FuseCanvas, int enabled);
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
/* NULL clears. The name is the fuse_id of a text box in the current ID scope.
 * Focus queries and textbox_edit must run inside that same scope. */
void       fuse_focus(FuseCanvas, const char *name);
bool       fuse_focused(FuseCanvas, const char *name);

/* Exhaustion/unbalanced/duplicate construction fails the whole build: NULL,
 * zero commands, canceled capture/focus/queued input, no synthesized release.
 * Error is sticky through draw, cleared by the next canvas_clear. */
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
/* After opening a stable named div, scope explicitly named child IDs to its ID.
 * Call before child IDs/queries. Ordinary nested divs inherit; a nested scope
 * combines its own scoped ID with child names. div_end restores the outer scope.
 * No scope means names remain canvas-global (legacy behavior). Anonymous child
 * IDs already derive from parent IDs. Requires an open div; no extra storage. */
void fuse_div_scope(FuseCanvas);
/* Temporarily enter a named child scope without emitting a layout element.
 * Resolves name against the current scope, exactly like id + div_scope.
 * Pair with restore(previous) in LIFO order before closing enclosing divs or
 * building other panels. Tokens are canvas-local; clear resets this override.
 * Intended for focus/edit queries outside panel building; no allocation. */
uint32_t fuse_scope_enter(FuseCanvas, const char *name);
void fuse_scope_restore(FuseCanvas, uint32_t previous);
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

/* Standalone named div wrappers; persistent docking API is below. */
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
// id functions stem the next element with a name in the current div ID scope.
// Hover/focus/edit queries resolve names in that same scope.
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

/* Annular menu, 1..8 slices. Slice 0 is centered to the right; indices
 * increase clockwise in screen coordinates. Inner/outer boundaries cancel;
 * angular boundaries belong to the following slice. Invalid/nonfinite input
 * returns -1. Pointer coordinates are relative to the same parent as center.
 * The caller owns capture, release, labels and actions. No input is consumed.
 * Drawing uses at most 144 triangle commands; no heap allocation. Link libm.
 * Rebuild consumers and size canvas buffers with fuse_canvas_memory:
 * FuseCmd grew by one float. The menu is a fixed-size drawing leaf. */
int fuse_radial_pick(float cx, float cy, float inner, float outer, int count, float px, float py);
int fuse_radial(FuseCanvas, float cx, float cy, float inner, float outer, int count,
    float px, float py, uint32_t idle, uint32_t hovered);

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

/* Copyright (c) 2026 Vasco Alves. Caller-backed, bounded docking layouts. */
#ifndef FUSE_DOCK_H
#define FUSE_DOCK_H

#define FUSE_DOCK_MAX FUSE_WINDOWS_MAX
#define FUSE_DOCK_NODES (2 * FUSE_DOCK_MAX - 1)
#define FUSE_DOCK_TITLE 24.0f

typedef struct FuseDock FuseDock;
typedef struct FuseDockRect { float x, y, w, h; } FuseDockRect;
typedef enum {
	FUSE_DOCK_CENTER, FUSE_DOCK_LEFT, FUSE_DOCK_RIGHT,
	FUSE_DOCK_TOP, FUSE_DOCK_BOTTOM, FUSE_DOCK_FLOAT
} FuseDockSide;
/* Node kind: 0 unused, 1 group, 2 horizontal children, 3 vertical children.
 * Group tabs are registration indices; file formats must map stable names.
 * Floating group rectangles are fractions of the host, not pixels. */
typedef struct FuseDockNode {
	int kind, a, b, count, active, floating, order;
	int tabs[FUSE_DOCK_MAX];
	float ratio;
	FuseDockRect rect;
} FuseDockNode;
typedef struct FuseDockLayout {
	int root;
	FuseDockNode nodes[FUSE_DOCK_NODES];
} FuseDockLayout;
/* Pointer coordinates are absolute host coordinates. cancel covers Escape,
 * focus loss and modal activation. previous/next automatically activate tabs
 * only after a tab/title click or fuse_dock_focus_tabs; content clicks leave
 * keys to the panel.
 * activate consumes Enter/Space without changing an already selected tab. */
typedef struct FuseDockEvent {
	float x, y;
	int press, release, cancel, previous, next, activate;
} FuseDockEvent;

size_t fuse_dock_memory(void);
FuseDock *fuse_dock_create(void *memory, size_t size, int capacity);
/* Returns registration index, or -1. Names/titles are copied, 31/63 bytes max. */
int fuse_dock_register(FuseDock *, const char *name, const char *title, float min_w, float min_h);
const char *fuse_dock_name(const FuseDock *, int window);
const char *fuse_dock_title(const FuseDock *, int window);
void fuse_dock_reset(FuseDock *);
/* Returns 0 without mutation for invalid targets/minima or while captured.
 * A zero-by-zero host permits constructing defaults before host sizing.
 * Cancel any pending gesture before programmatically changing placement. */
int fuse_dock_place(FuseDock *, int window, int target, FuseDockSide side);
/* Host changes cancel a captured gesture and retain its prepress arrangement. */
void fuse_dock_layout(FuseDock *, FuseDockRect host);
/* Returns 1 for consumed input, including the terminating release/cancel. */
int fuse_dock_event(FuseDock *, const FuseDockEvent *);
int fuse_dock_busy(const FuseDock *);
int fuse_dock_revision(const FuseDock *);
int fuse_dock_focused(const FuseDock *);
/* Give tabs keyboard focus (e.g. host Tab navigation), activating window.
 * Returns 0 for invalid windows or during capture. Does not reorder tabs. */
int fuse_dock_focus_tabs(FuseDock *, int window);
int fuse_dock_hit(const FuseDock *, float x, float y);
/* Visible windows in paint order; -1 after the last. */
int fuse_dock_visible(const FuseDock *, int position);
int fuse_dock_content(const FuseDock *, int window, FuseDockRect *);
int fuse_dock_group_rect(const FuseDock *, int window, FuseDockRect *);
void fuse_dock_export(const FuseDock *, FuseDockLayout *);
/* Every registered window must occur exactly once; float orders must be unique.
 * Rejects without mutation, including focus/capture. Success ends capture. */
int fuse_dock_import(FuseDock *, const FuseDockLayout *);
/* Builder takes absolute host coordinates and clips content. Its named root
 * scopes explicitly named children and keeps anonymous child IDs stable across
 * placement and emission order. Outside building, enter the registered window
 * name with fuse_scope_enter in the same enclosing scope for focus/edit queries. */
int fuse_dock_begin(FuseDock *, FuseCanvas, int window, const FuseClass *);
void fuse_dock_end(FuseCanvas);
/* Presentation colors are per-call widget arguments, never retained.
 * accent draws a 1px active-tab underline and a 2px keyboard-focus outline;
 * pass zero to omit those decorations. Layout/input geometry is unchanged. */
void fuse_dock_chrome(FuseDock *, FuseCanvas, int window,
	uint32_t background, uint32_t inactive, uint32_t active, uint32_t text,
	uint32_t grip, uint32_t accent);
void fuse_dock_feedback(FuseDock *, FuseCanvas, uint32_t color);
#endif

#endif /* FUSE_H */

#ifdef FUSE_IMPLEMENTATION
#undef FUSE_IMPLEMENTATION
#ifndef FUSE_IMPLEMENTATION_ONCE
#define FUSE_IMPLEMENTATION_ONCE

/* Core implementation. */
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#if defined(FUSE_DEBUG)
#define FASSERT_N(_1, _2, N, ...) N
#define FASSERT(...) FASSERT_N(__VA_ARGS__, FASSERT2, FASSERT1)(__VA_ARGS__)
#define FASSERT1(a) assert(a)
#define FASSERT2(a, s) assert((a) && (s))
#else
#define FASSERT(...) ((void)0)
#endif

#define FUSE_MEM_ALIGN 8
#define FUSE_CMDS_PER_EL 4
#define FUSE_CLIP_MAX 16
#define FUSE_SCREEN_MAX 32
#define FUSE_EDGE_NONE 0
#define FUSE_EDGE_PRESSED 1
#define FUSE_EDGE_RELEASED 2
#define FUSE_EL_DIV 1
#define FUSE_EL_BUTTON 2
#define FUSE_EL_SLIDER 4
#define FUSE_EL_RECT 8
#define FUSE_EL_SCROLL 16
#define FUSE_EL_COLS 32
#define FUSE_EL_COL 64
#define FUSE_EL_TEXT 128
#define FUSE_EL_IMAGE 256
#define FUSE_EL_SPACE 512
#define FUSE_EL_FIELD 1024
#define FUSE_EL_RADIAL 2048
#define FUSE_EL_ID_SCOPE 4096
#define FUSE_PI 3.14159265358979323846f
#define FUSE_FIELD_MAX 8
#define FUSE_INP_MAX 32
#define FUSE_INP_BYTES 512
#define FUSE_INP_KEY 1
#define FUSE_INP_TEXT 2
#define FUSE_SIZING_INHERIT 0x0F
#define FUSE_COL_MAX 8
#define FUSE_COL_STACK 8
#define FUSE_NAME_MAX 12
#define FUSE_NAME_SLOTS 192
#define FUSE_DEBUG_ROWS 48
#define FUSE_DEBUG_PANEL 168
#define FUSE_DEBUG_ROW_H 8
#define FUSE_DEBUG_HEAD 32
#define FUSE_DBG_PANEL  0xFF161616u
#define FUSE_DBG_TEXT   0xFFE6E6E6u
#define FUSE_DBG_DIM    0xFF8A8A8Au
#define FUSE_DBG_HOVER  0xFFFF9900u
#define FUSE_DBG_SELECT 0xFFFFFFFFu
#define FUSE_DBG_ROW    0xFF2C2C2Cu
#define FUSE_DBG_ROWSEL 0xFF3A2A10u
#define FUSE_DBG_PAD    0xFF3DDC97u
#define FUSE_GLYPH(r0, r1, r2, r3, r4, r5, r6) \
    ((uint64_t)(r0) | ((uint64_t)(r1) << 5) | ((uint64_t)(r2) << 10) | \
     ((uint64_t)(r3) << 15) | ((uint64_t)(r4) << 20) | ((uint64_t)(r5) << 25) | \
     ((uint64_t)(r6) << 30))

typedef struct FuseScreen {
    uint32_t element;
} FuseScreen;

typedef struct FuseElement {
    float *scroll_ptr;
    const FuseClass *cls;
    float x, y, w, h;
    float nob_pos;
    float scroll;
    uint32_t id;
    uint32_t color;
    uint32_t color_alt;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t child_count;
    uint16_t flags;
    uint8_t name_i; /* 0 = none, else names[name_i - 1] */
    uint8_t sizing; /* low nibble width, high nibble height; 0xF inherits */
} FuseElement;

typedef struct FuseDebugRow {
    uint32_t id;
    int16_t depth;
    int16_t x, y, w, h;
    char kind[6];
    char name[FUSE_NAME_MAX];
} FuseDebugRow;

typedef struct FuseHashItem {
    uint32_t id, generation;
    float x, y, w, h;
} FuseHashItem;

typedef struct FuseColGroup {
    uint32_t row;
    uint32_t index;
    uint32_t n;
    uint32_t col[FUSE_COL_MAX];
} FuseColGroup;

typedef struct FuseInp {
    uint8_t kind;
    uint8_t key;
    uint16_t n;
    uint16_t off;
} FuseInp;

typedef struct FuseMem {
    size_t total;
    size_t hash_off;
    size_t hash_cap;
    size_t elements_off;
    size_t screens_off;
    size_t screen_cap;
    size_t cmds_off;
    size_t cmd_cap;
} FuseMem;

struct fuse_canvas_t {
    FuseElement *elements;
    FuseScreen *screens;
    FuseCmd *cmds;
    FuseHashItem *hash;

    FuseColGroup col_group[FUSE_COL_STACK];

    size_t max_elements, max_windows;
    size_t hash_cap, cmd_cap, screen_cap;

    FuseInp inp[FUSE_INP_MAX];
    FuseFieldRun fields[FUSE_FIELD_MAX];
    char inp_bytes[FUSE_INP_BYTES];

    int input_enabled, input_canceled;
    int16_t layer, emit_layer;
    uint32_t layer_elements[64];
    int16_t layers[64];
    uint32_t layer_count;
    uint32_t element_count, cmd_count;
    uint32_t screen_count, scope_peak;
    uint32_t generation, capture_id, pending_id;
    uint32_t focus_id, focus_frame;
    uint32_t named_scope; /* 0 inherits the builder's div scope. */

    float width, height, pointer_x, pointer_y;
    float wheel_x, wheel_y;
    float clip[FUSE_CLIP_MAX][4];
    float debug_scroll;

    uint32_t wheel_capture, clip_n;
    int32_t pointer_state, pointer_edge;

    uint32_t debug_selected;
    uint32_t debug_total;

    FuseError error;

    char pending_name[FUSE_NAME_MAX];
    char names[FUSE_NAME_SLOTS][FUSE_NAME_MAX];

    uint8_t debug;
    uint8_t name_n;
    uint8_t col_n;
    uint8_t pending_sizing;
    uint8_t pending_w_sizing;
    uint8_t pending_h_sizing;
    uint16_t inp_n;
    uint16_t inp_len;
    uint8_t field_n;
};

static size_t fuse_internal_align(size_t n);
static size_t fuse_internal_hash_cap(size_t n);
static int fuse_internal_mem_add(size_t *off, size_t count, size_t width);
static int fuse_internal_mem(FuseMem *m, const FuseParams *params);
static size_t fuse_internal_max_elements(size_t bufsize);
static void fuse_internal_hash_keep_last(FuseCanvas c);
static FuseHashItem *fuse_internal_hash_slot(FuseCanvas c, uint32_t id, int vacant_ok);
static uint32_t fuse_internal_hash_str(const char *s);
static uint32_t fuse_internal_hash_mix(uint32_t a, uint32_t b);
static uint32_t fuse_internal_scoped_id(FuseCanvas c, uint32_t id);
static int fuse_internal_ok(FuseCanvas c);
static void fuse_internal_fail(FuseCanvas c, FuseError err);
static void fuse_internal_reset_frame(FuseCanvas c, int bump_gen);
static int16_t fuse_internal_layer(FuseCanvas c, uint32_t index);
static uint32_t fuse_internal_take_id(FuseCanvas c, uint32_t parent_id, uint32_t sibling);
static uint32_t fuse_internal_add_element(FuseCanvas c, uint32_t parent, uint32_t id, float x, float y, float w, float h, uint16_t flags);
static uint32_t fuse_internal_open_el(FuseCanvas c, float x, float y, float w, float h, uint16_t flags);
static void fuse_internal_drop_sizing(FuseCanvas c);
static void fuse_internal_apply_sizing(FuseCanvas c, FuseElement *el);
static uint8_t fuse_internal_mode(const FuseElement *ch, int axis, uint8_t cls_sizing);
static int fuse_internal_child_px(FuseElement *ch, int axis, uint8_t cls_sizing, float cls_v, float declared, float inner, float mn, float mx);
static void fuse_internal_center_label(FuseCanvas c, FuseElement *btn);
static float fuse_internal_percent(FuseCanvas c, float p, int yaxis);
static FuseHashItem *fuse_internal_last_box(FuseCanvas c, uint32_t id);
static int fuse_internal_geom_hit(FuseCanvas c, uint32_t id);
static int fuse_internal_last_hit(FuseCanvas c, uint32_t id);
static float fuse_internal_clamp01(float t);
static float fuse_internal_axis_size(uint8_t sizing, float declared, float cls_value, float inner);
static float fuse_internal_clamp_axis(float v, float mn, float mx);
static int fuse_px_i(float v);
static float fuse_px_scale(float s);
static void fuse_px_box(float *x, float *y, float *w, float *h);
static void fuse_internal_layout_class(FuseCanvas c, FuseElement *el);
static void fuse_internal_layout_cols(FuseCanvas c, FuseElement *el);
static int fuse_col_content_h(FuseCanvas c, FuseElement *col);
static void fuse_internal_layout(FuseCanvas c, uint32_t index);
static int fuse_internal_rect_visible(FuseCanvas c, float x, float y, float w, float h);
static FuseCmd *fuse_internal_emit(FuseCanvas c, uint8_t type, uint32_t id);
static void fuse_internal_emit_rect(FuseCanvas c, uint32_t id, float x, float y, float w, float h, uint32_t color);
static void fuse_internal_emit_clip(FuseCanvas c, uint8_t type, float x, float y, float w, float h);
static void fuse_internal_emit_radial(FuseCanvas c, const FuseElement *el, float x, float y);
static void fuse_internal_emit_tree(FuseCanvas c, uint32_t index, float ox, float oy);
static void fuse_internal_emit_children(FuseCanvas c, FuseElement *el, float ox, float oy);
static uint32_t fuse_internal_open_maybe_anon(FuseCanvas c, float x, float y, float w, float h, uint16_t flags);
static uint32_t fuse_internal_div_open(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls, uint8_t flags);
static int fuse_internal_scroll_axis(FuseElement *el);
static float fuse_internal_content_end(FuseCanvas c, FuseElement *el, int main_ax);
static void fuse_internal_scroll_clamp(FuseCanvas c, FuseElement *el);
static int fuse_internal_clip_apply(FuseCanvas c, float *x, float *y, float *w, float *h);
static void fuse_internal_clip_push(FuseCanvas c, float x, float y, float w, float h);
static void fuse_internal_clip_pop(FuseCanvas c);
static void fuse_internal_emit_scrollbar(FuseCanvas c, FuseElement *el, float cx, float cy);
static uint64_t fuse_internal_glyph(int c);
static void fuse_input_clear(FuseCanvas c);
static int fuse_utf8_prev(const char *s, int i);
static int fuse_utf8_next(const char *s, int i);
static int fuse_text_insert(char *buf, int cap, int *caret, const char *s, int n);
static int fuse_textbox_apply(FuseCanvas c, char *buf, int cap, int *caret,
    int (*allow)(unsigned char ch, void *user), void *user);
static void fuse_copy_n(char *dst, int cap, const char *src);
static void fuse_name_index(char *dst, const char *src, int index);
static int fuse_debug_over_panel(FuseCanvas c);
static void fuse_internal_debug(FuseCanvas c);

/* 5x7, row bits with MSB = left pixel. */
static const uint64_t fuse_font[128] = {
    [' '] = 0,
    ['-'] = FUSE_GLYPH(0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00),
    ['.'] = FUSE_GLYPH(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04),
    ['/'] = FUSE_GLYPH(0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10),
    ['0'] = FUSE_GLYPH(0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E),
    ['1'] = FUSE_GLYPH(0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E),
    ['2'] = FUSE_GLYPH(0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F),
    ['3'] = FUSE_GLYPH(0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E),
    ['4'] = FUSE_GLYPH(0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02),
    ['5'] = FUSE_GLYPH(0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E),
    ['6'] = FUSE_GLYPH(0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E),
    ['7'] = FUSE_GLYPH(0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08),
    ['8'] = FUSE_GLYPH(0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E),
    ['9'] = FUSE_GLYPH(0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C),
    [':'] = FUSE_GLYPH(0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00),
    ['<'] = FUSE_GLYPH(0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08),
    ['>'] = FUSE_GLYPH(0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02),
    ['A'] = FUSE_GLYPH(0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    ['B'] = FUSE_GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E),
    ['C'] = FUSE_GLYPH(0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E),
    ['D'] = FUSE_GLYPH(0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E),
    ['E'] = FUSE_GLYPH(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F),
    ['F'] = FUSE_GLYPH(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10),
    ['G'] = FUSE_GLYPH(0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E),
    ['H'] = FUSE_GLYPH(0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    ['I'] = FUSE_GLYPH(0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E),
    ['J'] = FUSE_GLYPH(0x0F, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C),
    ['K'] = FUSE_GLYPH(0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11),
    ['L'] = FUSE_GLYPH(0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F),
    ['M'] = FUSE_GLYPH(0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11),
    ['N'] = FUSE_GLYPH(0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11),
    ['O'] = FUSE_GLYPH(0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    ['P'] = FUSE_GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10),
    ['Q'] = FUSE_GLYPH(0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D),
    ['R'] = FUSE_GLYPH(0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11),
    ['S'] = FUSE_GLYPH(0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E),
    ['T'] = FUSE_GLYPH(0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04),
    ['U'] = FUSE_GLYPH(0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    ['V'] = FUSE_GLYPH(0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04),
    ['W'] = FUSE_GLYPH(0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11),
    ['X'] = FUSE_GLYPH(0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11),
    ['Y'] = FUSE_GLYPH(0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04),
    ['Z'] = FUSE_GLYPH(0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F),
    ['_'] = FUSE_GLYPH(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F),
};

static size_t
fuse_internal_align(size_t n)
{
    return (n + (FUSE_MEM_ALIGN - 1)) & ~(size_t)(FUSE_MEM_ALIGN - 1);
}

static size_t
fuse_internal_hash_cap(size_t n)
{
	size_t cap = 8;
	/* Validated element bounds keep doubling representable on 32-bit too. */
	while (cap < n * 2)
		cap *= 2;
	return cap;
}

static int
fuse_internal_mem_add(size_t *off, size_t count, size_t width)
{
	size_t next;
	if (count > (SIZE_MAX - *off) / width)
		return 0;
	next = *off + count * width;
	if (next > SIZE_MAX - (FUSE_MEM_ALIGN - 1))
		return 0;
	*off = fuse_internal_align(next);
	return 1;
}

static int
fuse_internal_mem(FuseMem *m, const FuseParams *params)
{
	size_t off;
	if (!m || !params || !params->max_elements || params->max_elements > UINT32_MAX / FUSE_CMDS_PER_EL ||
		!params->max_windows || params->max_windows > FUSE_WINDOWS_MAX ||
		!params->max_scopes || params->max_scopes > UINT32_MAX)
		return 0;
	m->hash_cap = fuse_internal_hash_cap(params->max_elements);
	m->cmd_cap = params->max_elements * FUSE_CMDS_PER_EL;
	m->screen_cap = params->max_scopes;
	off = fuse_internal_align(sizeof(struct fuse_canvas_t));
	m->hash_off = off;
	if (!fuse_internal_mem_add(&off, m->hash_cap, sizeof(FuseHashItem))) return 0;
	m->elements_off = off;
	if (!fuse_internal_mem_add(&off, params->max_elements, sizeof(FuseElement))) return 0;
	m->screens_off = off;
	if (!fuse_internal_mem_add(&off, m->screen_cap, sizeof(FuseScreen))) return 0;
	m->cmds_off = off;
	if (!fuse_internal_mem_add(&off, m->cmd_cap, sizeof(FuseCmd))) return 0;
	m->total = off;
	return 1;
}

static size_t
fuse_internal_max_elements(size_t bufsize)
{
    size_t lo, hi, mid, best;
    FuseMem m;
    if (bufsize < fuse_canvas_memory(1))
        return 0;
    lo = 1;
    hi = bufsize / 32;
    if (hi > UINT32_MAX / FUSE_CMDS_PER_EL)
        hi = UINT32_MAX / FUSE_CMDS_PER_EL;
    if (hi < 1)
        hi = 1;
    best = 0;
    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;
        FuseParams params = {mid, FUSE_WINDOWS_MAX, mid < FUSE_SCREEN_MAX ? mid : FUSE_SCREEN_MAX};
        if (fuse_internal_mem(&m, &params) && m.total <= bufsize) {
            best = mid;
            lo = mid + 1;
        } else {
            if (mid == 0)
                break;
            hi = mid - 1;
        }
    }
    return best;
}

static void
fuse_internal_hash_keep_last(FuseCanvas c)
{
    FuseHashItem *scratch, *slot;
    size_t i, nkeep, keep_gen;
    nkeep = 0;
    keep_gen = c->generation - 1;
    scratch = (FuseHashItem *)c->cmds;
    for (i = 0; i < c->hash_cap; i++) {
        if (c->hash[i].id != 0 && c->hash[i].generation == keep_gen) {
            scratch[nkeep] = c->hash[i];
            nkeep++;
        }
    }
    memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
    for (i = 0; i < nkeep; i++) {
        slot = fuse_internal_hash_slot(c, scratch[i].id, 1);
        if (slot)
            *slot = scratch[i];
    }
}

static FuseHashItem *
fuse_internal_hash_slot(FuseCanvas c, uint32_t id, int vacant_ok)
{
    size_t i, mask, start;
    if (id == 0 || !c->hash)
        return NULL;
    mask = c->hash_cap - 1;
    start = id & mask;
    i = start;
    for (;;) {
        if (c->hash[i].id == id)
            return &c->hash[i];
        if (c->hash[i].id == 0)
            return vacant_ok ? &c->hash[i] : NULL;
        i = (i + 1) & mask;
        if (i == start)
            return NULL;
    }
}

static uint32_t
fuse_internal_hash_str(const char *s)
{
    uint32_t h;
    h = 2166136261u;
    if (!s)
        return 1;
    while (*s) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
        s++;
    }
    if (h == 0)
        h = 1;
    return h;
}

static uint32_t
fuse_internal_hash_mix(uint32_t a, uint32_t b)
{
    uint32_t h;
    h = 2166136261u;
    h ^= a;
    h *= 16777619u;
    h ^= b;
    h *= 16777619u;
    if (h == 0)
        h = 1;
    return h;
}

/* Nearest explicitly scoped div; ordinary nested layout does not change IDs. */
static uint32_t
fuse_internal_scoped_id(FuseCanvas c, uint32_t id)
{
    uint32_t i;
    FuseElement *el;

    if (c->named_scope)
        return fuse_internal_hash_mix(c->named_scope, id);
    for (i = c->screen_count; i > 1; i--) {
        el = &c->elements[c->screens[i - 1].element];
        if (el->flags & FUSE_EL_ID_SCOPE)
            return fuse_internal_hash_mix(el->id, id);
    }
    return id;
}

static int
fuse_internal_ok(FuseCanvas c)
{
    return c && c->error == FUSE_ERR_OK;
}

static void
fuse_internal_fail(FuseCanvas c, FuseError err)
{
    if (c && c->error == FUSE_ERR_OK) {
        c->error = err;
        c->cmd_count = 0;
        c->capture_id = c->focus_id = c->focus_frame = 0;
        c->wheel_capture = 0;
        c->pointer_edge = FUSE_EDGE_NONE;
        c->input_canceled = c->pointer_state == FUSE_POINTER_PRESSED;
        c->inp_n = c->inp_len = c->field_n = 0;
    }
}

static void
fuse_internal_reset_frame(FuseCanvas c, int bump_gen)
{
    if (bump_gen) {
        c->generation++;
        if (c->generation == 0) {
            memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
            c->generation = 1;
        } else {
            fuse_internal_hash_keep_last(c);
        }
    }
    c->input_enabled = 1;
    c->layer_count = 0;
    c->layer = c->emit_layer = 0;
    c->element_count = 1;
    c->cmd_count = 0;
    c->screen_count = c->scope_peak = 1;
    c->named_scope = 0;
    c->pending_id = 0;
    c->pending_name[0] = 0;
    c->pending_sizing = 0;
    c->name_n = 0;
    c->col_n = 0;
    c->error = FUSE_ERR_OK;
    memset(&c->elements[0], 0, sizeof c->elements[0]);
    c->elements[0].w = c->width;
    c->elements[0].h = c->height;
    c->screens[0].element = 0;
    c->focus_frame = c->focus_id;
    c->field_n = 0;
}

static uint32_t
fuse_internal_take_id(FuseCanvas c, uint32_t parent_id, uint32_t sibling)
{
    uint32_t id, i;
    id = c->pending_id;
    c->pending_id = 0;
    if (id == 0)
        id = fuse_internal_hash_mix(parent_id, sibling);
    for (i = 0; i < c->element_count; i++) {
        if (c->elements[i].id == id) {
            c->pending_name[0] = 0;
            fuse_internal_drop_sizing(c);
            fuse_internal_fail(c, FUSE_ERR_DUPLICATE_ID);
            return 0;
        }
    }
    return id;
}

static uint32_t
fuse_internal_add_element(FuseCanvas c, uint32_t parent, uint32_t id, float x, float y, float w, float h, uint16_t flags)
{
    FuseElement *el, *p;
    uint32_t index;
    if (c->element_count >= c->max_elements) {
        fuse_internal_drop_sizing(c);
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return 0;
    }
    index = c->element_count;
    c->element_count++;
    el = &c->elements[index];
    memset(el, 0, sizeof *el);
    fuse_px_box(&x, &y, &w, &h);
    el->id = id;
    el->x = x;
    el->y = y;
    el->w = w;
    el->h = h;
    el->flags = flags;
    el->name_i = 0;
    fuse_internal_apply_sizing(c, el);
    if (id != 0 && c->pending_name[0] && c->name_n < FUSE_NAME_SLOTS) {
        fuse_copy_n(c->names[c->name_n], FUSE_NAME_MAX, c->pending_name);
        c->name_n++;
        el->name_i = c->name_n;
    }
    c->pending_name[0] = 0;
    p = &c->elements[parent];
    if (p->last_child != 0) {
        c->elements[p->last_child].next_sibling = index;
    } else {
        p->first_child = index;
    }
    p->last_child = index;
    p->child_count++;
    return index;
}

static uint32_t
fuse_internal_open_el(FuseCanvas c, float x, float y, float w, float h, uint16_t flags)
{
    uint32_t parent_el, sibling, id;
    parent_el = c->screens[c->screen_count - 1].element;
    sibling = c->elements[parent_el].child_count;
    id = fuse_internal_take_id(c, c->elements[parent_el].id, sibling);
    if (!fuse_internal_ok(c))
        return 0;
    return fuse_internal_add_element(c, parent_el, id, x, y, w, h, flags);
}

static FuseHashItem *
fuse_internal_last_box(FuseCanvas c, uint32_t id)
{
    FuseHashItem *item;
    item = fuse_internal_hash_slot(c, id, 0);
    if (!item || item->generation + 1 != c->generation)
        return NULL;
    return item;
}

static int
fuse_internal_geom_hit(FuseCanvas c, uint32_t id)
{
    FuseHashItem *item;
    if (!c->input_enabled) return 0;
    item = fuse_internal_last_box(c, id);
    if (!item)
        return 0;
    if (c->pointer_x < item->x || c->pointer_y < item->y)
        return 0;
    if (c->pointer_x > item->x + item->w || c->pointer_y > item->y + item->h)
        return 0;
    return 1;
}

static int
fuse_internal_last_hit(FuseCanvas c, uint32_t id)
{
    if (c->debug)
        return 0;
    return fuse_internal_geom_hit(c, id);
}

static float
fuse_internal_clamp01(float t)
{
    if (t < 0.0f)
        return 0.0f;
    if (t > 1.0f)
        return 1.0f;
    return t;
}

static float
fuse_internal_axis_size(uint8_t sizing, float declared, float cls_value, float inner)
{
    switch (sizing) {
    case FUSE_SIZING_FIXED:
        return cls_value;
    case FUSE_SIZING_PERCENT:
        return cls_value * inner;
    case FUSE_SIZING_GROW:
        return 0.0f;
    case FUSE_SIZING_FIT: /* FALLTHROUGH */
    default:
        return declared;
    }
}

static float
fuse_internal_clamp_axis(float v, float mn, float mx)
{
    if (v < mn)
        v = mn;
    if (mx > 0.0f && v > mx)
        v = mx;
    return v;
}

static int
fuse_px_i(float v)
{
    if (v >= 0.0f)
        return (int)(v + 0.5f);
    return (int)(v - 0.5f);
}

static float
fuse_px_scale(float s)
{
    int p;

    p = fuse_px_i(s);
    if (p < 1)
        p = 1;
    return (float)p;
}

static void
fuse_px_box(float *x, float *y, float *w, float *h)
{
    int x0;
    int y0;
    int x1;
    int y1;

    x0 = fuse_px_i(*x);
    y0 = fuse_px_i(*y);
    x1 = fuse_px_i(*x + *w);
    y1 = fuse_px_i(*y + *h);
    if (x1 < x0)
        x1 = x0;
    if (y1 < y0)
        y1 = y0;
    *x = (float)x0;
    *y = (float)y0;
    *w = (float)(x1 - x0);
    *h = (float)(y1 - y0);
}

static void
fuse_internal_drop_sizing(FuseCanvas c)
{
    c->pending_sizing = 0;
}

static uint8_t
fuse_internal_own_mode(const FuseElement *ch, int axis)
{
    uint8_t mode;

    if (axis == 0)
        mode = (uint8_t)(ch->sizing & 0x0F);
    else
        mode = (uint8_t)((ch->sizing >> 4) & 0x0F);
    return mode;
}

static void
fuse_internal_apply_sizing(FuseCanvas c, FuseElement *el)
{
    uint8_t w;
    uint8_t h;

    w = FUSE_SIZING_INHERIT;
    h = FUSE_SIZING_INHERIT;
    if (c->pending_sizing) {
        w = c->pending_w_sizing;
        h = c->pending_h_sizing;
        c->pending_sizing = 0;
    }
    el->sizing = (uint8_t)((h << 4) | (w & 0x0F));
}

static uint8_t
fuse_internal_mode(const FuseElement *ch, int axis, uint8_t cls_sizing)
{
    uint8_t mode;

    mode = fuse_internal_own_mode(ch, axis);
    if (mode == FUSE_SIZING_INHERIT || mode > FUSE_SIZING_PERCENT)
        return cls_sizing;
    return mode;
}

static int
fuse_internal_child_px(FuseElement *ch, int axis, uint8_t cls_sizing, float cls_v,
                       float declared, float inner, float mn, float mx)
{
    uint8_t mode;
    uint8_t own;
    float v;

    own = fuse_internal_own_mode(ch, axis);
    mode = fuse_internal_mode(ch, axis, cls_sizing);
    if (mode == FUSE_SIZING_GROW)
        return 0;
    if (own == FUSE_SIZING_INHERIT)
        v = fuse_internal_axis_size(mode, declared, cls_v, inner);
    else if (mode == FUSE_SIZING_PERCENT)
        v = declared * inner;
    else if (mode == FUSE_SIZING_FIXED)
        v = declared;
    else
        v = declared;
    v = fuse_internal_clamp_axis(v, mn, mx);
    if (v < 0.0f)
        v = 0.0f;
    return fuse_px_i(v);
}

static void
fuse_internal_center_label(FuseCanvas c, FuseElement *btn)
{
    FuseElement *ch;
    int x;
    int y;

    if (!btn || btn->first_child == 0)
        return;
    ch = &c->elements[btn->first_child];
    if (!(ch->flags & FUSE_EL_TEXT))
        return;
    x = (fuse_px_i(btn->w) - fuse_px_i(ch->w)) / 2;
    y = (fuse_px_i(btn->h) - fuse_px_i(ch->h)) / 2;
    ch->x = (float)x;
    ch->y = (float)y;
}

static void
fuse_internal_layout_class(FuseCanvas c, FuseElement *el)
{
    const FuseClass *cls;
    uint32_t n, grow_n, idx;
    float cls_v[2], mn[2], mx[2];
    uint8_t sizing[2], align_main, align_cross;
    int main_ax, cross_ax;
    int inner_i[2];
    int gap_i, sum_i, leftover_i, grow_i, rem_i, extra_i, cursor_i;
    int pad_l, pad_r, pad_t, pad_b, pad_main, pad_cross;
    FuseElement *ch;

    cls = el->cls;
    n = el->child_count;
    if (!cls || n == 0)
        return;
    fuse_px_box(&el->x, &el->y, &el->w, &el->h);
    pad_l = fuse_px_i(cls->pad_l);
    pad_r = fuse_px_i(cls->pad_r);
    pad_t = fuse_px_i(cls->pad_t);
    pad_b = fuse_px_i(cls->pad_b);
    if (pad_l < 0) pad_l = 0;
    if (pad_r < 0) pad_r = 0;
    if (pad_t < 0) pad_t = 0;
    if (pad_b < 0) pad_b = 0;
    /* Column padding and the gap between columns belong to the row.
     * Inside a column the same class only stacks children. */
    if (el->flags & FUSE_EL_COL) {
        pad_l = 0;
        pad_r = 0;
        pad_t = 0;
        pad_b = 0;
    }
    inner_i[0] = (int)el->w - pad_l - pad_r;
    inner_i[1] = (int)el->h - pad_t - pad_b;
    if (inner_i[0] < 0)
        inner_i[0] = 0;
    if (inner_i[1] < 0)
        inner_i[1] = 0;
    if (el->flags & FUSE_EL_COL)
        main_ax = 1;
    else
        main_ax = cls->direction != FUSE_DIRECTION_COLUMN ? 0 : 1;
    cross_ax = 1 - main_ax;
    sizing[0] = cls->width_sizing;
    sizing[1] = cls->height_sizing;
    cls_v[0] = cls->width;
    cls_v[1] = cls->height;
    mn[0] = cls->min_width;
    mn[1] = cls->min_height;
    mx[0] = cls->max_width;
    mx[1] = cls->max_height;
    align_main = main_ax == 0 ? cls->align_x : cls->align_y;
    align_cross = main_ax == 0 ? cls->align_y : cls->align_x;
    gap_i = fuse_px_i(cls->gap);
    if (gap_i < 0)
        gap_i = 0;
    sum_i = 0;
    grow_n = 0;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        float declared[2];
        int sz[2];

        ch = &c->elements[idx];
        declared[0] = ch->w;
        declared[1] = ch->h;
        sz[0] = fuse_internal_child_px(ch, 0, sizing[0], cls_v[0], declared[0],
                                       (float)inner_i[0], mn[0], mx[0]);
        sz[1] = fuse_internal_child_px(ch, 1, sizing[1], cls_v[1], declared[1],
                                       (float)inner_i[1], mn[1], mx[1]);
        ch->w = (float)sz[0];
        ch->h = (float)sz[1];
        if (fuse_internal_mode(ch, main_ax, sizing[main_ax]) == FUSE_SIZING_GROW)
            grow_n++;
        else
            sum_i += sz[main_ax];
    }
    leftover_i = inner_i[main_ax] - sum_i - gap_i * (int)(n - 1);
    grow_i = 0;
    rem_i = 0;
    if (grow_n > 0) {
        if (leftover_i > 0) {
            grow_i = leftover_i / (int)grow_n;
            rem_i = leftover_i % (int)grow_n;
        }
        leftover_i = 0;
    }
    extra_i = leftover_i > 0 ? leftover_i : 0;
    pad_main = main_ax == 0 ? pad_l : pad_t;
    pad_cross = main_ax == 0 ? pad_t : pad_l;
    cursor_i = pad_main;
    if (align_main == FUSE_ALIGN_CENTER)
        cursor_i += extra_i / 2;
    else if (align_main == FUSE_ALIGN_END)
        cursor_i += extra_i;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        int sz[2];
        int pos[2];
        int cross;
        int g;

        ch = &c->elements[idx];
        sz[0] = (int)ch->w;
        sz[1] = (int)ch->h;
        if (fuse_internal_mode(ch, main_ax, sizing[main_ax]) == FUSE_SIZING_GROW) {
            g = grow_i;
            if (rem_i > 0) {
                g++;
                rem_i--;
            }
            sz[main_ax] = fuse_px_i(fuse_internal_clamp_axis((float)g, mn[main_ax], mx[main_ax]));
            if (sz[main_ax] < 0)
                sz[main_ax] = 0;
        }
        if (fuse_internal_mode(ch, cross_ax, sizing[cross_ax]) == FUSE_SIZING_GROW) {
            sz[cross_ax] = fuse_px_i(fuse_internal_clamp_axis(
                (float)inner_i[cross_ax], mn[cross_ax], mx[cross_ax]));
            if (sz[cross_ax] < 0)
                sz[cross_ax] = 0;
        }
        cross = pad_cross;
        if (align_cross == FUSE_ALIGN_CENTER)
            cross += (inner_i[cross_ax] - sz[cross_ax]) / 2;
        else if (align_cross == FUSE_ALIGN_END)
            cross += inner_i[cross_ax] - sz[cross_ax];
        pos[main_ax] = cursor_i;
        pos[cross_ax] = cross;
        ch->x = (float)pos[0];
        ch->y = (float)pos[1];
        ch->w = (float)sz[0];
        ch->h = (float)sz[1];
        cursor_i += sz[main_ax] + gap_i;
    }
}

static int
fuse_internal_scroll_axis(FuseElement *el)
{
    if (el && el->cls && el->cls->direction != FUSE_DIRECTION_COLUMN)
        return 0;
    return 1;
}

static float
fuse_internal_content_end(FuseCanvas c, FuseElement *el, int main_ax)
{
    uint32_t idx;
    float end;
    float pad;

    end = 0.0f;
    pad = 0.0f;
    if (el->cls)
        pad = main_ax == 0 ? el->cls->pad_r : el->cls->pad_b;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        FuseElement *ch;
        float e;

        ch = &c->elements[idx];
        e = main_ax == 0 ? ch->x + ch->w : ch->y + ch->h;
        if (e > end)
            end = e;
    }
    return end + pad;
}

static void
fuse_internal_scroll_clamp(FuseCanvas c, FuseElement *el)
{
    int main_ax;
    float content;
    float view;
    float max_scroll;
    float delta;

    if (c->debug && fuse_debug_over_panel(c))
        return;
    main_ax = fuse_internal_scroll_axis(el);
    content = fuse_internal_content_end(c, el, main_ax);
    view = main_ax == 0 ? el->w : el->h;
    max_scroll = content - view;
    if (max_scroll < 0.0f)
        max_scroll = 0.0f;
    if (el->scroll_ptr && el->id == c->wheel_capture) {
        delta = main_ax == 0 ? c->wheel_x : c->wheel_y;
        if (main_ax == 0 && delta == 0.0f)
            delta = c->wheel_y;
        *el->scroll_ptr -= delta;
        c->wheel_x = 0.0f;
        c->wheel_y = 0.0f;
    }
    if (el->scroll_ptr) {
        if (*el->scroll_ptr < 0.0f)
            *el->scroll_ptr = 0.0f;
        if (*el->scroll_ptr > max_scroll)
            *el->scroll_ptr = max_scroll;
        el->scroll = *el->scroll_ptr;
    } else {
        el->scroll = 0.0f;
    }
}

static int
fuse_col_content_h(FuseCanvas c, FuseElement *col)
{
    const FuseClass *cls;
    uint32_t idx;
    int gap;
    int n;
    int sum;
    float end;

    cls = col->cls;
    if (!cls) {
        end = 0.0f;
        for (idx = col->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
            FuseElement *ch;
            float e;

            ch = &c->elements[idx];
            e = ch->y + ch->h;
            if (e > end)
                end = e;
        }
        return fuse_px_i(end);
    }
    gap = fuse_px_i(cls->gap);
    if (gap < 0)
        gap = 0;
    n = 0;
    sum = 0;
    for (idx = col->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        int h;

        h = fuse_px_i(c->elements[idx].h);
        if (h < 0)
            h = 0;
        sum += h;
        n++;
    }
    if (n == 0)
        return 0;
    return sum + gap * (n - 1);
}

static void
fuse_internal_layout_cols(FuseCanvas c, FuseElement *el)
{
    const FuseClass *cls;
    uint32_t idx;
    int pad_l, pad_r, pad_t, pad_b, gap;
    int inner_w, inner_h, cursor, rem, n, i, equal;
    float sum_r;

    cls = el->cls;
    pad_l = 0;
    pad_r = 0;
    pad_t = 0;
    pad_b = 0;
    gap = 0;
    if (cls) {
        pad_l = fuse_px_i(cls->pad_l);
        pad_r = fuse_px_i(cls->pad_r);
        pad_t = fuse_px_i(cls->pad_t);
        pad_b = fuse_px_i(cls->pad_b);
        gap = fuse_px_i(cls->gap);
    }
    if (pad_l < 0) pad_l = 0;
    if (pad_r < 0) pad_r = 0;
    if (pad_t < 0) pad_t = 0;
    if (pad_b < 0) pad_b = 0;
    if (gap < 0) gap = 0;
    n = (int)el->child_count;
    if (n <= 0)
        return;
    sum_r = 0.0f;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        float r;

        r = c->elements[idx].nob_pos;
        if (r < 0.0f)
            r = 0.0f;
        sum_r += r;
    }
    equal = 0;
    if (sum_r <= 0.0f) {
        equal = 1;
        sum_r = (float)n;
    }
    inner_w = (int)el->w - pad_l - pad_r - gap * (n - 1);
    if (inner_w < 0)
        inner_w = 0;
    inner_h = (int)el->h - pad_t - pad_b;
    if (inner_h < 0)
        inner_h = 0;
    cursor = pad_l;
    rem = inner_w;
    i = 0;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        FuseElement *col;
        float r;
        int w;

        col = &c->elements[idx];
        r = col->nob_pos;
        if (r < 0.0f)
            r = 0.0f;
        if (equal)
            r = 1.0f;
        if (i == n - 1)
            w = rem;
        else {
            w = (int)((float)inner_w * (r / sum_r));
            if (w < 0)
                w = 0;
            if (w > rem)
                w = rem;
        }
        col->x = (float)cursor;
        col->y = (float)pad_t;
        col->w = (float)w;
        col->h = (float)inner_h;
        cursor += w + gap;
        rem -= w;
        i++;
    }
}

static void
fuse_internal_layout(FuseCanvas c, uint32_t index)
{
    FuseElement *el;
    uint32_t child;
    el = &c->elements[index];
    if (el->flags & FUSE_EL_COLS)
        fuse_internal_layout_cols(c, el);
    else if (el->cls)
        fuse_internal_layout_class(c, el);
    if (el->flags & FUSE_EL_BUTTON)
        fuse_internal_center_label(c, el);
    for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
        fuse_internal_layout(c, child);
    if (el->flags & FUSE_EL_SCROLL)
        fuse_internal_scroll_clamp(c, el);
}

static int
fuse_internal_rect_visible(FuseCanvas c, float x, float y, float w, float h)
{
    if (w <= 0.0f || h <= 0.0f)
        return 0;
    if (x + w <= 0.0f || y + h <= 0.0f)
        return 0;
    if (x >= c->width || y >= c->height)
        return 0;
    return 1;
}

static int
fuse_internal_clip_apply(FuseCanvas c, float *x, float *y, float *w, float *h)
{
    float cx, cy, cw, ch, x1, y1, cx1, cy1;

    if (!c || c->clip_n == 0)
        return 1;
    cx = c->clip[c->clip_n - 1][0];
    cy = c->clip[c->clip_n - 1][1];
    cw = c->clip[c->clip_n - 1][2];
    ch = c->clip[c->clip_n - 1][3];
    x1 = *x + *w;
    y1 = *y + *h;
    cx1 = cx + cw;
    cy1 = cy + ch;
    if (*x < cx) *x = cx;
    if (*y < cy) *y = cy;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    *w = x1 - *x;
    *h = y1 - *y;
    if (*w <= 0.0f || *h <= 0.0f) {
        *w = 0.0f;
        *h = 0.0f;
        return 0;
    }
    return 1;
}

static void
fuse_internal_clip_push(FuseCanvas c, float x, float y, float w, float h)
{
    if (c->clip_n >= FUSE_CLIP_MAX)
        return;
    if (!fuse_internal_clip_apply(c, &x, &y, &w, &h)) {
        w = 0.0f;
        h = 0.0f;
    }
    fuse_px_box(&x, &y, &w, &h);
    c->clip[c->clip_n][0] = x;
    c->clip[c->clip_n][1] = y;
    c->clip[c->clip_n][2] = w;
    c->clip[c->clip_n][3] = h;
    c->clip_n++;
}

static void
fuse_internal_clip_pop(FuseCanvas c)
{
    if (c->clip_n > 1)
        c->clip_n--;
}

static void
fuse_internal_emit_scrollbar(FuseCanvas c, FuseElement *el, float cx, float cy)
{
    int main_ax;
    float content;
    float view;
    float max_scroll;
    float t;
    float nob;
    float bar;

    main_ax = fuse_internal_scroll_axis(el);
    content = fuse_internal_content_end(c, el, main_ax);
    view = main_ax == 0 ? el->w : el->h;
    if (content <= view)
        return;
    max_scroll = content - view;
    t = el->scroll / max_scroll;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    bar = 4.0f;
    if (main_ax == 1) {
        nob = view * view / content;
        if (nob < 8.0f)
            nob = 8.0f;
        if (nob > view)
            nob = view;
        fuse_internal_emit_rect(c, 0, cx + el->w - bar - 1.0f, cy, bar, view, 0xFF222222u);
        fuse_internal_emit_rect(c, 0, cx + el->w - bar - 1.0f, cy + t * (view - nob), bar, nob, 0xFF888888u);
    } else {
        nob = view * view / content;
        if (nob < 8.0f)
            nob = 8.0f;
        if (nob > view)
            nob = view;
        fuse_internal_emit_rect(c, 0, cx, cy + el->h - bar - 1.0f, view, bar, 0xFF222222u);
        fuse_internal_emit_rect(c, 0, cx + t * (view - nob), cy + el->h - bar - 1.0f, nob, bar, 0xFF888888u);
    }
}

static FuseCmd *
fuse_internal_emit(FuseCanvas c, uint8_t type, uint32_t id)
{
    FuseCmd *cmd;
    if (c->cmd_count >= c->cmd_cap) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return NULL;
    }
    cmd = &c->cmds[c->cmd_count];
    c->cmd_count++;
    memset(cmd, 0, sizeof *cmd);
    cmd->type = type;
    cmd->id = id;
    cmd->z = c->emit_layer;
    return cmd;
}

static void
fuse_internal_emit_rect(FuseCanvas c, uint32_t id, float x, float y, float w, float h, uint32_t color)
{
    FuseCmd *cmd;

    fuse_px_box(&x, &y, &w, &h);
    if (!fuse_internal_rect_visible(c, x, y, w, h))
        return;
    cmd = fuse_internal_emit(c, FUSE_CMD_RECT, id);
    if (!cmd)
        return;
    cmd->rect.x = x;
    cmd->rect.y = y;
    cmd->rect.w = w;
    cmd->rect.h = h;
    cmd->rect.color = color;
}

static void
fuse_internal_emit_clip(FuseCanvas c, uint8_t type, float x, float y, float w, float h)
{
    FuseCmd *cmd;

    fuse_px_box(&x, &y, &w, &h);
    cmd = fuse_internal_emit(c, type, 0);
    if (!cmd)
        return;
    cmd->clip.x = x;
    cmd->clip.y = y;
    cmd->clip.w = w;
    cmd->clip.h = h;
}

static void
fuse_internal_emit_children(FuseCanvas c, FuseElement *el, float ox, float oy)
{
    uint32_t child;
    for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
        fuse_internal_emit_tree(c, child, ox, oy);
}

static void
fuse_internal_emit_radial(FuseCanvas c, const FuseElement *el, float x, float y)
{
    int slice, step, half, count = (int)el->child_count;
    int steps = (64 + count - 1) / count;
    float span = 2.0f * FUSE_PI / count;
    float inner = el->nob_pos, outer = el->w * 0.5f;
    float a, b, ix0, iy0, ix1, iy1, ox0, oy0, ox1, oy1;
    FuseCmd *cmd;

    x += outer;
    y += outer;
    for (slice = 0; slice < count; slice++) {
        for (step = 0; step < steps; step++) {
            a = (slice - 0.5f + (float)step / steps) * span;
            b = (slice - 0.5f + (float)(step + 1) / steps) * span;
            ix0 = x + inner * cosf(a); iy0 = y + inner * sinf(a);
            ix1 = x + inner * cosf(b); iy1 = y + inner * sinf(b);
            ox0 = x + outer * cosf(a); oy0 = y + outer * sinf(a);
            ox1 = x + outer * cosf(b); oy1 = y + outer * sinf(b);
            for (half = 0; half < 2; half++) {
                if (!(cmd = fuse_internal_emit(c, FUSE_CMD_TRIANGLE, el->id)))
                    return;
                cmd->triangle = half ?
                    (FuseCmdTriangle){ix0, iy0, ox1, oy1, ix1, iy1, 0} :
                    (FuseCmdTriangle){ix0, iy0, ox0, oy0, ox1, oy1, 0};
                cmd->triangle.color = slice == (int)el->scroll ? el->color_alt : el->color;
            }
        }
    }
}

static void
fuse_internal_emit_tree(FuseCanvas c, uint32_t index, float ox, float oy)
{
    FuseElement *el;
    FuseHashItem *item;
    float cx, cy, nob_w, nob_x;
    float bx, by, bw, bh;
    el = &c->elements[index];
    c->emit_layer = fuse_internal_layer(c, index);
    cx = ox + el->x;
    cy = oy + el->y;
    if (el->id != 0) {
        item = fuse_internal_hash_slot(c, el->id, 1);
        if (item) {
            bx = cx;
            by = cy;
            bw = el->w;
            bh = el->h;
            fuse_internal_clip_apply(c, &bx, &by, &bw, &bh);
            fuse_px_box(&bx, &by, &bw, &bh);
            item->id = el->id;
            item->generation = c->generation;
            item->x = bx;
            item->y = by;
            item->w = bw;
            item->h = bh;
        } else {
            fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
            return;
        }
    }
    if (index == 0) {
        fuse_internal_emit_children(c, el, cx, cy);
        return;
    }
    if (el->flags & FUSE_EL_DIV) {
        float cox, coy;

        if (el->color != 0)
            fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_START, cx, cy, el->w, el->h);
        fuse_internal_clip_push(c, cx, cy, el->w, el->h);
        cox = cx;
        coy = cy;
        if (el->flags & FUSE_EL_SCROLL) {
            if (fuse_internal_scroll_axis(el) == 0)
                cox -= el->scroll;
            else
                coy -= el->scroll;
        }
        fuse_internal_emit_children(c, el, cox, coy);
        c->emit_layer = fuse_internal_layer(c, index);
        if (el->flags & FUSE_EL_SCROLL)
            fuse_internal_emit_scrollbar(c, el, cx, cy);
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_END, cx, cy, el->w, el->h);
        fuse_internal_clip_pop(c);
        return;
    }
    if (el->flags & FUSE_EL_TEXT) {
        fuse_internal_emit_children(c, el, cx, cy);
        return;
    }
    if (el->flags & FUSE_EL_RADIAL) {
        fuse_internal_emit_radial(c, el, cx, cy);
        return;
    }
    if (el->flags & FUSE_EL_IMAGE) {
        FuseCmd *cmd;

        cmd = fuse_internal_emit(c, FUSE_CMD_IMAGE, el->id);
        if (!cmd)
            return;
        cmd->image.x = cx;
        cmd->image.y = cy;
        cmd->image.w = el->w;
        cmd->image.h = el->h;
        cmd->image.handle = el->color;
        return;
    }
    if (el->flags & FUSE_EL_FIELD) {
        FuseCmd *cmd;
        float ix, iw;
        uint32_t slot;

        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        slot = el->last_child;
        if (slot == 0 || slot > c->field_n)
            return;
        slot--;
        ix = cx + el->scroll;
        iw = el->w - el->scroll - el->nob_pos;
        if (iw < 1.0f)
            iw = 1.0f;
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_START, ix, cy, iw, el->h);
        fuse_internal_clip_push(c, ix, cy, iw, el->h);
        cmd = fuse_internal_emit(c, FUSE_CMD_TEXT, el->id);
        if (cmd) {
            cmd->text.x = ix;
            cmd->text.y = cy;
            cmd->text.w = iw;
            cmd->text.h = el->h;
            cmd->text.slot = slot;
        }
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_END, ix, cy, iw, el->h);
        fuse_internal_clip_pop(c);
        return;
    }
    if (el->flags & FUSE_EL_BUTTON) {
        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        fuse_internal_emit_children(c, el, cx, cy);
        return;
    }
    if (el->flags & FUSE_EL_SPACE)
        return;
    if (el->flags & FUSE_EL_RECT) {
        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        return;
    }
    if (el->flags & FUSE_EL_SLIDER) {
        nob_w = el->h;
        if (nob_w > el->w)
            nob_w = el->w;
        nob_x = cx + el->nob_pos * (el->w - nob_w);
        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        fuse_internal_emit_rect(c, el->id, nob_x, cy, nob_w, el->h, el->color_alt);
    }
}

static const uint32_t fuse_debug_palette[6] = {
    0xFFE06A6Au,
    0xFF6AE06Au,
    0xFF6A9AE0u,
    0xFFE0D06Au,
    0xFFD06AE0u,
    0xFF6AE0D0u,
};

static void
fuse_copy_n(char *dst, int cap, const char *src)
{
    int i;

    if (cap <= 0)
        return;
    if (!src)
        src = "";
    i = 0;
    while (src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void
fuse_cat(char *dst, int cap, const char *src)
{
    int n;

    if (!dst || cap <= 0)
        return;
    n = 0;
    while (dst[n])
        n++;
    if (!src)
        return;
    while (*src && n < cap - 1)
        dst[n++] = *src++;
    dst[n] = 0;
}

static void
fuse_cat_i(char *dst, int cap, int v)
{
    char num[16];
    char one[2];
    int i;
    unsigned u;

    if (v == (int)0x80000000) {
        fuse_cat(dst, cap, "-2147483648");
        return;
    }
    if (v < 0) {
        fuse_cat(dst, cap, "-");
        u = (unsigned)(-v);
    } else {
        u = (unsigned)v;
    }
    i = 0;
    if (u == 0)
        num[i++] = '0';
    while (u && i < 15) {
        num[i++] = (char)('0' + (u % 10));
        u /= 10;
    }
    one[1] = 0;
    while (i > 0) {
        one[0] = num[--i];
        fuse_cat(dst, cap, one);
    }
}

static void
fuse_name_index(char *dst, const char *src, int index)
{
    fuse_copy_n(dst, FUSE_NAME_MAX, src);
    fuse_cat(dst, FUSE_NAME_MAX, ".");
    fuse_cat_i(dst, FUSE_NAME_MAX, index);
}

static float
fuse_debug_panel_w(FuseCanvas c)
{
    float w;

    w = (float)FUSE_DEBUG_PANEL;
    if (c->width < w + 80.0f)
        w = c->width * 0.5f;
    w = (float)fuse_px_i(w);
    if (w < 1.0f)
        w = c->width;
    if (w > c->width)
        w = c->width;
    return w;
}

static int
fuse_debug_over_panel(FuseCanvas c)
{
    float x;
    float w;

    if (!c || !c->debug || c->width < 8.0f || c->height < 8.0f)
        return 0;
    w = fuse_debug_panel_w(c);
    x = c->width - w;
    if (c->pointer_x < x || c->pointer_y < 0.0f)
        return 0;
    if (c->pointer_x > c->width || c->pointer_y > c->height)
        return 0;
    return 1;
}

static int
fuse_debug_fit(FuseCanvas c)
{
    int fit;
    int list;

    list = fuse_px_i(c->height) - FUSE_DEBUG_HEAD;
    fit = list / FUSE_DEBUG_ROW_H;
    if (fit < 0)
        fit = 0;
    if (fit > FUSE_DEBUG_ROWS)
        fit = FUSE_DEBUG_ROWS;
    return fit;
}

static int
fuse_debug_show(const FuseElement *el)
{
    if (el->flags & (FUSE_EL_DIV | FUSE_EL_BUTTON | FUSE_EL_SLIDER | FUSE_EL_TEXT | FUSE_EL_IMAGE | FUSE_EL_SPACE | FUSE_EL_FIELD))
        return 1;
    if ((el->flags & FUSE_EL_RECT) && el->id != 0)
        return 1;
    return 0;
}

static void
fuse_debug_kind(const FuseElement *el, char *dst)
{
    const char *s;

    if (el->flags & FUSE_EL_FIELD)
        s = "field";
    else if (el->flags & FUSE_EL_IMAGE)
        s = "img";
    else if (el->flags & FUSE_EL_SPACE)
        s = "spc";
    else if (el->flags & FUSE_EL_TEXT)
        s = "txt";
    else if (el->flags & FUSE_EL_COLS)
        s = "cols";
    else if (el->flags & FUSE_EL_COL)
        s = "col";
    else if (el->flags & FUSE_EL_BUTTON)
        s = "btn";
    else if (el->flags & FUSE_EL_SLIDER)
        s = "sld";
    else if (el->flags & FUSE_EL_SCROLL)
        s = "scr";
    else if (el->flags & FUSE_EL_DIV)
        s = "div";
    else if (el->flags & FUSE_EL_RECT)
        s = "rect";
    else
        s = "el";
    fuse_copy_n(dst, 6, s);
}

static void
fuse_debug_clip_reset(FuseCanvas c)
{
    c->clip_n = 1;
    c->clip[0][0] = 0.0f;
    c->clip[0][1] = 0.0f;
    c->clip[0][2] = c->width;
    c->clip[0][3] = c->height;
}

static int
fuse_debug_emit_rect(FuseCanvas c, float x, float y, float w, float h, uint32_t color)
{
    FuseCmd *cmd;

    fuse_px_box(&x, &y, &w, &h);
    if (!fuse_internal_clip_apply(c, &x, &y, &w, &h))
        return 0;
    fuse_px_box(&x, &y, &w, &h);
    if (!fuse_internal_rect_visible(c, x, y, w, h))
        return 0;
    if (c->cmd_count >= c->cmd_cap)
        return 0;
    cmd = &c->cmds[c->cmd_count];
    c->cmd_count++;
    memset(cmd, 0, sizeof *cmd);
    cmd->type = FUSE_CMD_RECT;
    cmd->rect.x = x;
    cmd->rect.y = y;
    cmd->rect.w = w;
    cmd->rect.h = h;
    cmd->rect.color = color;
    return 1;
}

static void
fuse_debug_border(FuseCanvas c, float x, float y, float w, float h, uint32_t color)
{
    if (w < 1.0f || h < 1.0f)
        return;
    fuse_debug_emit_rect(c, x, y, w, 1.0f, color);
    if (h > 1.0f)
        fuse_debug_emit_rect(c, x, y + h - 1.0f, w, 1.0f, color);
    if (h > 2.0f)
        fuse_debug_emit_rect(c, x, y + 1.0f, 1.0f, h - 2.0f, color);
    if (w > 1.0f && h > 2.0f)
        fuse_debug_emit_rect(c, x + w - 1.0f, y + 1.0f, 1.0f, h - 2.0f, color);
}

static int
fuse_debug_inside(FuseCanvas c, float x, float y, float w, float h)
{
    float bx, by, bw, bh;

    bx = x;
    by = y;
    bw = w;
    bh = h;
    if (!fuse_internal_clip_apply(c, &bx, &by, &bw, &bh))
        return 0;
    if (c->pointer_x < bx || c->pointer_y < by)
        return 0;
    if (c->pointer_x > bx + bw || c->pointer_y > by + bh)
        return 0;
    return 1;
}

static void
fuse_debug_text(FuseCanvas c, float x, float y, const char *str, uint32_t color, float max_x)
{
    float cx;

    if (!str)
        return;
    x = (float)fuse_px_i(x);
    y = (float)fuse_px_i(y);
    cx = x;
    while (*str) {
        uint64_t g;
        int row;

        if (max_x > 0.0f && cx >= max_x)
            return;
        g = fuse_internal_glyph((unsigned char)*str);
        for (row = 0; row < 7; row++) {
            uint32_t bits;
            int col;

            bits = (uint32_t)((g >> (row * 5)) & 0x1F);
            col = 0;
            while (col < 5) {
                if (bits & (1u << (4 - col))) {
                    int start;

                    start = col;
                    while (col < 5 && (bits & (1u << (4 - col))))
                        col++;
                    if (max_x <= 0.0f || cx + (float)start < max_x)
                        fuse_debug_emit_rect(c, cx + (float)start, y + (float)row,
                                             (float)(col - start), 1.0f, color);
                } else {
                    col++;
                }
            }
        }
        cx += 6.0f;
        str++;
    }
}

typedef struct FuseDebugBuild {
    FuseDebugRow *rows;
    FuseDebugRow sel;
    int fit;
    int row_n;
    int hot_row;
    int block;
    int draw;
    int have_sel;
    uint32_t seen;
    uint32_t hover;
} FuseDebugBuild;

static void
fuse_debug_fill(FuseCanvas c, FuseDebugRow *row, const FuseElement *el, int depth, float x, float y)
{
    memset(row, 0, sizeof *row);
    row->id = el->id;
    row->depth = (int16_t)depth;
    row->x = (int16_t)fuse_px_i(x);
    row->y = (int16_t)fuse_px_i(y);
    row->w = (int16_t)fuse_px_i(el->w);
    row->h = (int16_t)fuse_px_i(el->h);
    fuse_debug_kind(el, row->kind);
    if (el->name_i)
        fuse_copy_n(row->name, FUSE_NAME_MAX, c->names[el->name_i - 1]);
}

static void
fuse_debug_visit(FuseCanvas c, uint32_t index, float ox, float oy, int depth, FuseDebugBuild *b)
{
    FuseElement *el;
    float cx, cy;
    uint32_t color;
    int show;

    el = &c->elements[index];
    cx = ox + el->x;
    cy = oy + el->y;
    show = index != 0 && fuse_debug_show(el);
    if (show) {
        b->seen++;
        if (el->id != 0 && el->id == c->debug_selected) {
            fuse_debug_fill(c, &b->sel, el, depth, cx, cy);
            b->have_sel = 1;
        }
        if (!b->draw) {
            if ((int)b->seen > (int)c->debug_scroll && b->row_n < b->fit) {
                if (b->hot_row == b->row_n && el->id != 0)
                    b->hover = el->id;
                fuse_debug_fill(c, &b->rows[b->row_n], el, depth, cx, cy);
                b->row_n++;
            }
            if (!b->block && el->id != 0 && fuse_debug_inside(c, cx, cy, el->w, el->h))
                b->hover = el->id;
        } else if (el->w >= 1.0f && el->h >= 1.0f) {
            color = fuse_debug_palette[depth % 6];
            if (el->id != 0 && el->id == c->debug_selected)
                color = FUSE_DBG_SELECT;
            if (el->id != 0 && el->id == b->hover)
                color = FUSE_DBG_HOVER;
            fuse_debug_border(c, cx, cy, el->w, el->h, color);
            if (el->id != 0 && el->id == c->debug_selected && el->cls &&
                (el->flags & FUSE_EL_DIV)) {
                int pl, pr, pt, pb;
                float iw, ih;

                pl = fuse_px_i(el->cls->pad_l);
                pr = fuse_px_i(el->cls->pad_r);
                pt = fuse_px_i(el->cls->pad_t);
                pb = fuse_px_i(el->cls->pad_b);
                if (pl < 0) pl = 0;
                if (pr < 0) pr = 0;
                if (pt < 0) pt = 0;
                if (pb < 0) pb = 0;
                iw = el->w - (float)(pl + pr);
                ih = el->h - (float)(pt + pb);
                if (pl + pr + pt + pb > 0 && iw >= 1.0f && ih >= 1.0f)
                    fuse_debug_border(c, cx + (float)pl, cy + (float)pt, iw, ih, FUSE_DBG_PAD);
            }
        }
    }
    if (el->flags & FUSE_EL_DIV) {
        float cox, coy;
        uint32_t child;
        uint32_t n0;

        n0 = c->clip_n;
        fuse_internal_clip_push(c, cx, cy, el->w, el->h);
        cox = cx;
        coy = cy;
        if (el->flags & FUSE_EL_SCROLL) {
            if (fuse_internal_scroll_axis(el) == 0)
                cox -= el->scroll;
            else
                coy -= el->scroll;
        }
        for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
            fuse_debug_visit(c, child, cox, coy, depth + 1, b);
        if (c->clip_n > n0)
            fuse_internal_clip_pop(c);
        return;
    }
    if (index == 0 || el->first_child != 0) {
        uint32_t child;

        for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
            fuse_debug_visit(c, child, cx, cy, depth + 1, b);
    }
}

static void
fuse_debug_panel(FuseCanvas c, FuseDebugBuild *b)
{
    float pw, px, max_x, y;
    int i;
    char line[48];

    pw = fuse_debug_panel_w(c);
    px = c->width - pw;
    fuse_debug_clip_reset(c);
    fuse_debug_emit_rect(c, px, 0.0f, pw, c->height, FUSE_DBG_PANEL);
    fuse_debug_border(c, px, 0.0f, pw, c->height, FUSE_DBG_HOVER);
    max_x = c->width - 8.0f;
    fuse_internal_clip_push(c, px, 0.0f, pw, c->height);
    fuse_debug_text(c, px + 6.0f, 8.0f, "FUSE", FUSE_DBG_TEXT, max_x);
    if (!b->have_sel) {
        fuse_debug_text(c, px + 6.0f, 16.0f, "click an element", FUSE_DBG_DIM, max_x);
    } else {
        line[0] = 0;
        fuse_cat(line, (int)sizeof line, b->sel.kind);
        if (b->sel.name[0]) {
            fuse_cat(line, (int)sizeof line, " ");
            fuse_cat(line, (int)sizeof line, b->sel.name);
        }
        fuse_debug_text(c, px + 6.0f, 16.0f, line, FUSE_DBG_TEXT, max_x);
        line[0] = 0;
        fuse_cat_i(line, (int)sizeof line, b->sel.x);
        fuse_cat(line, (int)sizeof line, ",");
        fuse_cat_i(line, (int)sizeof line, b->sel.y);
        fuse_cat(line, (int)sizeof line, " ");
        fuse_cat_i(line, (int)sizeof line, b->sel.w);
        fuse_cat(line, (int)sizeof line, "x");
        fuse_cat_i(line, (int)sizeof line, b->sel.h);
        fuse_debug_text(c, px + 6.0f, 24.0f, line, FUSE_DBG_DIM, max_x);
    }
    y = (float)FUSE_DEBUG_HEAD;
    for (i = 0; i < b->row_n; i++) {
        FuseDebugRow *row;
        int indent;
        float tx;
        float size_x;
        char size[24];
        uint32_t bg;

        row = &b->rows[i];
        bg = FUSE_DBG_ROW;
        if (row->id != 0 && row->id == c->debug_selected)
            bg = FUSE_DBG_ROWSEL;
        if (i == b->hot_row)
            bg = FUSE_DBG_HOVER;
        fuse_debug_emit_rect(c, px + 1.0f, y, pw - 2.0f, (float)FUSE_DEBUG_ROW_H, bg);
        indent = row->depth;
        if (indent > 8)
            indent = 8;
        tx = px + 4.0f + (float)indent * 6.0f;
        size[0] = 0;
        fuse_cat_i(size, (int)sizeof size, row->w);
        fuse_cat(size, (int)sizeof size, "x");
        fuse_cat_i(size, (int)sizeof size, row->h);
        size_x = max_x - fuse_text_width(size, 1.0f);
        fuse_debug_text(c, size_x, y, size, FUSE_DBG_DIM, max_x);
        fuse_debug_text(c, tx, y, row->kind, FUSE_DBG_TEXT, size_x - 4.0f);
        if (row->name[0])
            fuse_debug_text(c, tx + 24.0f, y, row->name, FUSE_DBG_TEXT, size_x - 4.0f);
        y += (float)FUSE_DEBUG_ROW_H;
    }
    if (c->clip_n > 1)
        fuse_internal_clip_pop(c);
}

static void
fuse_debug_collect(FuseCanvas c, FuseDebugBuild *b)
{
    FuseDebugRow *rows;
    int attempt;
    int fit;
    int hot;
    int block;
    float max_scroll;

    rows = b->rows;
    fit = fuse_debug_fit(c);
    block = fuse_debug_over_panel(c);
    hot = -1;
    if (block && c->pointer_y >= (float)FUSE_DEBUG_HEAD) {
        hot = (int)((c->pointer_y - (float)FUSE_DEBUG_HEAD) / (float)FUSE_DEBUG_ROW_H);
        if (hot < 0 || hot >= fit)
            hot = -1;
    }
    for (attempt = 0; attempt < 2; attempt++) {
        memset(b, 0, sizeof *b);
        b->rows = rows;
        b->fit = fit;
        b->hot_row = hot;
        b->block = block;
        fuse_debug_clip_reset(c);
        fuse_debug_visit(c, 0, 0.0f, 0.0f, -1, b);
        c->debug_total = b->seen;
        max_scroll = 0.0f;
        if ((int)b->seen > fit)
            max_scroll = (float)((int)b->seen - fit);
        if (c->debug_scroll > max_scroll) {
            c->debug_scroll = max_scroll;
            continue;
        }
        break;
    }
}

static void
fuse_internal_debug(FuseCanvas c)
{
    FuseDebugRow rows[FUSE_DEBUG_ROWS];
    FuseDebugBuild b;

    if (c->width < 8.0f || c->height < 8.0f)
        return;
    if (fuse_debug_over_panel(c)) {
        c->debug_scroll -= c->wheel_y / (float)FUSE_DEBUG_ROW_H;
        c->wheel_x = 0.0f;
        c->wheel_y = 0.0f;
    }
    if (c->debug_scroll < 0.0f)
        c->debug_scroll = 0.0f;
    memset(&b, 0, sizeof b);
    b.rows = rows;
    b.hot_row = -1;
    fuse_debug_collect(c, &b);
    if (c->pointer_edge == FUSE_EDGE_RELEASED) {
        if (fuse_debug_over_panel(c)) {
            if (b.hot_row >= 0 && b.hot_row < b.row_n && b.rows[b.hot_row].id != 0)
                c->debug_selected = b.rows[b.hot_row].id;
        } else if (b.hover != 0) {
            c->debug_selected = b.hover;
        } else {
            c->debug_selected = 0;
        }
    }
    b.draw = 1;
    b.have_sel = 0;
    b.seen = 0;
    fuse_debug_clip_reset(c);
    fuse_debug_visit(c, 0, 0.0f, 0.0f, -1, &b);
    fuse_debug_panel(c, &b);
}

size_t
fuse_memory(const FuseParams *params)
{
	FuseMem m;
	return fuse_internal_mem(&m, params) ? m.total : 0;
}

FuseCanvas
fuse_place_in_memory(void *buf, size_t bytes, const FuseParams *params)
{
	FuseMem m;
	FuseCanvas c;
	unsigned char *p = buf;
	if (!buf || (uintptr_t)buf % FUSE_MEM_ALIGN || !fuse_internal_mem(&m, params) || bytes < m.total)
		return NULL;
	memset(p, 0, sizeof(struct fuse_canvas_t));
	c = (FuseCanvas)p;
	c->max_elements = params->max_elements;
	c->max_windows = params->max_windows;
	c->hash_cap = m.hash_cap;
	c->cmd_cap = m.cmd_cap;
	c->screen_cap = m.screen_cap;
	c->hash = (FuseHashItem *)(p + m.hash_off);
	c->elements = (FuseElement *)(p + m.elements_off);
	c->screens = (FuseScreen *)(p + m.screens_off);
	c->cmds = (FuseCmd *)(p + m.cmds_off);
	memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
	fuse_internal_reset_frame(c, 0);
	return c;
}

size_t
fuse_canvas_max_windows(FuseCanvas c)
{
	return c ? c->max_windows : 0;
}

size_t
fuse_canvas_scope_peak(FuseCanvas c)
{
	return c ? c->scope_peak : 0;
}

FuseError
fuse_canvas_error(FuseCanvas c)
{
	return c ? c->error : FUSE_ERR_BUF_TOO_SMALL;
}

size_t
fuse_canvas_memory(size_t max_elements)
{
	FuseParams params = {max_elements, FUSE_WINDOWS_MAX,
		max_elements < FUSE_SCREEN_MAX ? max_elements : FUSE_SCREEN_MAX};
	return fuse_memory(&params);
}

FuseCanvas
fuse_canvas_create(void *buf, size_t bufsize)
{
	size_t n = fuse_internal_max_elements(bufsize);
	FuseParams params = {n, FUSE_WINDOWS_MAX, n < FUSE_SCREEN_MAX ? n : FUSE_SCREEN_MAX};
	return fuse_place_in_memory(buf, bufsize, &params);
}

void
fuse_canvas_clear(FuseCanvas c)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    fuse_internal_reset_frame(c, 1);
}

void
fuse_canvas_resize(FuseCanvas c, float w, float h)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    c->width = (float)fuse_px_i(w);
    c->height = (float)fuse_px_i(h);
    if (c->width < 0.0f)
        c->width = 0.0f;
    if (c->height < 0.0f)
        c->height = 0.0f;
    if (c->element_count > 0) {
        c->elements[0].w = c->width;
        c->elements[0].h = c->height;
    }
}

void
fuse_canvas_pointer(FuseCanvas c, FusePointerState pointer_state, float x, float y)
{
    int prev;
    FASSERT(c, "null canvas");
    if (!c)
        return;
    prev = c->pointer_state;
    c->pointer_x = x;
    c->pointer_y = y;
    c->pointer_edge = FUSE_EDGE_NONE;
    if (pointer_state == FUSE_POINTER_PRESSED && prev != FUSE_POINTER_PRESSED)
        c->pointer_edge = FUSE_EDGE_PRESSED;
    else if (pointer_state == FUSE_POINTER_RELEASED && prev == FUSE_POINTER_PRESSED)
        c->pointer_edge = FUSE_EDGE_RELEASED;
    c->pointer_state = pointer_state;
    if (c->input_canceled) {
        c->pointer_edge = FUSE_EDGE_NONE;
        if (pointer_state != FUSE_POINTER_PRESSED)
            c->input_canceled = 0;
    }
}

void
fuse_canvas_wheel(FuseCanvas c, float dx, float dy)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    c->wheel_x += dx;
    c->wheel_y += dy;
}

static void
fuse_input_clear(FuseCanvas c)
{
    c->inp_n = 0;
    c->inp_len = 0;
}

void
fuse_canvas_text(FuseCanvas c, const char *utf8)
{
    int n, i;

    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c) || !utf8 || !utf8[0])
        return;
    n = 0;
    while (utf8[n])
        n++;
    if (c->inp_n >= FUSE_INP_MAX)
        return;
    if ((int)c->inp_len + n > FUSE_INP_BYTES)
        return;
    c->inp[c->inp_n].kind = FUSE_INP_TEXT;
    c->inp[c->inp_n].key = 0;
    c->inp[c->inp_n].n = (uint16_t)n;
    c->inp[c->inp_n].off = c->inp_len;
    for (i = 0; i < n; i++)
        c->inp_bytes[c->inp_len + (uint16_t)i] = utf8[i];
    c->inp_len = (uint16_t)(c->inp_len + n);
    c->inp_n++;
}

void
fuse_canvas_key(FuseCanvas c, FuseKey key)
{
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c) || key == 0)
        return;
    if (c->inp_n >= FUSE_INP_MAX)
        return;
    c->inp[c->inp_n].kind = FUSE_INP_KEY;
    c->inp[c->inp_n].key = (uint8_t)key;
    c->inp[c->inp_n].n = 0;
    c->inp[c->inp_n].off = 0;
    c->inp_n++;
}

uint32_t
fuse_scope_enter(FuseCanvas c, const char *name)
{
    uint32_t previous;
    FASSERT(c && name, "null scope");
    if (!c || !name)
        return 0;
    previous = c->named_scope;
    c->named_scope = fuse_internal_scoped_id(c, fuse_internal_hash_str(name));
    return previous;
}

void
fuse_scope_restore(FuseCanvas c, uint32_t previous)
{
    FASSERT(c, "null canvas");
    if (c)
        c->named_scope = previous;
}

void
fuse_focus(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    if (!name || !name[0])
        c->focus_id = 0;
    else
        c->focus_id = fuse_internal_scoped_id(c, fuse_internal_hash_str(name));
    c->focus_frame = c->focus_id;
}

bool
fuse_focused(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null focus name");
    if (!c || !name || !name[0] || c->focus_id == 0)
        return false;
    return c->focus_id == fuse_internal_scoped_id(c, fuse_internal_hash_str(name));
}

FuseCmd *
fuse_canvas_draw(FuseCanvas c, size_t *cmd_count)
{
    FASSERT(c, "null canvas");
    FASSERT(cmd_count, "null cmd_count");
    if (cmd_count)
        *cmd_count = 0;
    if (!c)
        return NULL;
    if (!fuse_internal_ok(c))
        goto done;
    if (c->screen_count != 1 || c->col_n != 0) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        goto done;
    }
    fuse_internal_layout(c, 0);
    c->cmd_count = 0;
    c->clip_n = 1;
    c->clip[0][0] = 0.0f;
    c->clip[0][1] = 0.0f;
    c->clip[0][2] = c->width;
    c->clip[0][3] = c->height;
    fuse_internal_emit_tree(c, 0, 0.0f, 0.0f);
    if (!fuse_internal_ok(c))
        goto done;
    if (c->debug)
        fuse_internal_debug(c);
    if (!fuse_internal_ok(c))
        goto done;
    if (cmd_count)
        *cmd_count = c->cmd_count;
done:
    c->wheel_x = 0.0f;
    c->wheel_y = 0.0f;
    c->wheel_capture = 0;
    fuse_input_clear(c);
    if (!fuse_internal_ok(c))
        return NULL;
    return c->cmds;
}

void
fuse_canvas_debug(FuseCanvas c, bool enabled)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    c->debug = enabled ? 1 : 0;
    if (c->debug)
        c->capture_id = 0;
}

static uint32_t
fuse_internal_div_open(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls, uint8_t flags)
{
    uint32_t index;

    if (!fuse_internal_ok(c))
        return 0;
    if (w <= 0.0f || h <= 0.0f)
        return 0;
    if (c->screen_count >= c->screen_cap) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return 0;
    }
    index = fuse_internal_open_el(c, x, y, w, h, flags);
    if (!fuse_internal_ok(c))
        return 0;
    c->elements[index].cls = cls;
    c->elements[index].color = cls ? cls->color : 0;
    c->screens[c->screen_count].element = index;
    c->screen_count++;
    return index;
}

void
fuse_div_begin(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls)
{
    FASSERT(c, "null canvas");
    FASSERT(w > 0.0f && h > 0.0f, "div w,h must be > 0");
    fuse_internal_div_open(c, x, y, w, h, cls, FUSE_EL_DIV);
}

void
fuse_div_begin_scroll(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls, float *scroll)
{
    uint32_t index;
    uint32_t id;

    FASSERT(c, "null canvas");
    FASSERT(scroll, "null scroll");
    FASSERT(w > 0.0f && h > 0.0f, "div w,h must be > 0");
    if (!scroll)
        return;
    index = fuse_internal_div_open(c, x, y, w, h, cls, (uint8_t)(FUSE_EL_DIV | FUSE_EL_SCROLL));
    if (!fuse_internal_ok(c) || index == 0)
        return;
    c->elements[index].scroll_ptr = scroll;
    c->elements[index].scroll = *scroll;
    id = c->elements[index].id;
    if (fuse_internal_geom_hit(c, id) && !fuse_debug_over_panel(c))
        c->wheel_capture = id;
}

void
fuse_div_scope(FuseCanvas c)
{
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    if (c->screen_count <= 1) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    c->elements[c->screens[c->screen_count - 1].element].flags |= FUSE_EL_ID_SCOPE;
}

void
fuse_div_end(FuseCanvas c)
{
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    if (c->screen_count <= 1) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    c->screen_count--;
}

static void
fuse_col_fit_row(FuseCanvas c, FuseColGroup *g)
{
    FuseElement *row;
    int content;
    int pad_t;
    int pad_b;
    int h;
    uint32_t i;

    row = &c->elements[g->row];
    content = 0;
    for (i = 0; i < g->n; i++) {
        int ch;

        ch = fuse_col_content_h(c, &c->elements[g->col[i]]);
        if (ch > content)
            content = ch;
    }
    pad_t = 0;
    pad_b = 0;
    if (row->cls) {
        pad_t = fuse_px_i(row->cls->pad_t);
        pad_b = fuse_px_i(row->cls->pad_b);
        if (pad_t < 0)
            pad_t = 0;
        if (pad_b < 0)
            pad_b = 0;
    }
    h = content + pad_t + pad_b;
    row->h = (float)h;
    for (i = 0; i < g->n; i++)
        c->elements[g->col[i]].h = (float)content;
}

void
fuse_col_begin(FuseCanvas c, float x, float y, float w, const FuseClass *cls, const float *ratios, uint32_t n)
{
    FuseColGroup *g;
    uint32_t row;
    uint32_t i;

    FASSERT(c, "null canvas");
    FASSERT(ratios, "null ratios");
    FASSERT(n > 0 && n <= FUSE_COL_MAX, "column count");
    FASSERT(w > 0.0f, "col w must be > 0");
    if (!c || !fuse_internal_ok(c))
        return;
    if (!ratios || n == 0 || n > FUSE_COL_MAX || w <= 0.0f || c->col_n >= FUSE_COL_STACK) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return;
    }
    row = fuse_internal_div_open(c, x, y, w, 1.0f, cls, (uint8_t)(FUSE_EL_DIV | FUSE_EL_COLS));
    if (!fuse_internal_ok(c) || row == 0)
        return;
    if (c->screen_count >= c->screen_cap) {
        c->screen_count--;
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return;
    }
    g = &c->col_group[c->col_n];
    g->row = row;
    g->index = 0;
    g->n = n;
    for (i = 0; i < n; i++) {
        uint32_t col;
        float ratio;

        ratio = ratios[i];
        if (ratio < 0.0f)
            ratio = 0.0f;
        col = fuse_internal_open_el(c, 0.0f, 0.0f, 1.0f, 1.0f, (uint8_t)(FUSE_EL_DIV | FUSE_EL_COL));
        if (!fuse_internal_ok(c) || col == 0) {
            c->screen_count--;
            return;
        }
        c->elements[col].cls = cls;
        c->elements[col].color = 0;
        c->elements[col].nob_pos = ratio;
        g->col[i] = col;
    }
    c->screens[c->screen_count].element = g->col[0];
    c->screen_count++;
    c->col_n++;
}

void
fuse_col_switch(FuseCanvas c)
{
    FuseColGroup *g;

    FASSERT(c, "null canvas");
    if (!c || !fuse_internal_ok(c))
        return;
    if (c->col_n == 0) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    g = &c->col_group[c->col_n - 1];
    if (c->screen_count < 2 || c->screens[c->screen_count - 1].element != g->col[g->index]) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    if (g->index + 1 >= g->n) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    g->index++;
    c->screens[c->screen_count - 1].element = g->col[g->index];
}

void
fuse_col_end(FuseCanvas c)
{
    FuseColGroup *g;

    FASSERT(c, "null canvas");
    if (!c || !fuse_internal_ok(c))
        return;
    if (c->col_n == 0) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    g = &c->col_group[c->col_n - 1];
    if (c->screen_count < 3 ||
        c->screens[c->screen_count - 1].element != g->col[g->index] ||
        c->screens[c->screen_count - 2].element != g->row) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    c->screen_count -= 2;
    fuse_col_fit_row(c, g);
    c->col_n--;
}

void
fuse_id(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null id name");
    if (!fuse_internal_ok(c))
        return;
    c->pending_id = fuse_internal_scoped_id(c, fuse_internal_hash_str(name));
    fuse_copy_n(c->pending_name, FUSE_NAME_MAX, name);
}

void
fuse_idi(FuseCanvas c, const char *name, int index)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null id name");
    if (!fuse_internal_ok(c))
        return;
    c->pending_id = fuse_internal_scoped_id(c,
        fuse_internal_hash_mix(fuse_internal_hash_str(name), (uint32_t)index));
    fuse_name_index(c->pending_name, name, index);
}

bool
fuse_element_is_hovered(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null hover name");
    if (!fuse_internal_ok(c) || !name)
        return false;
    return fuse_internal_last_hit(c,
        fuse_internal_scoped_id(c, fuse_internal_hash_str(name))) ? true : false;
}

bool
fuse_button(FuseCanvas c, float x, float y, float w, float h, uint32_t selected, uint32_t hovered)
{
    uint32_t index, id;
    int hit;
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return false;
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_BUTTON);
    if (!fuse_internal_ok(c))
        return false;
    id = c->elements[index].id;
    hit = fuse_internal_last_hit(c, id);
    c->elements[index].color = hit ? hovered : selected;
    return hit && c->pointer_edge == FUSE_EDGE_RELEASED && c->capture_id == 0;
}

void
fuse_slider(FuseCanvas c, float x, float y, float w, float h, uint32_t track, uint32_t nob, float *nob_pos)
{
    uint32_t index, id;
    int hit;
    FuseHashItem *item;
    FASSERT(c, "null canvas");
    FASSERT(nob_pos, "null nob_pos");
    if (!fuse_internal_ok(c) || !nob_pos)
        return;
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_SLIDER);
    if (!fuse_internal_ok(c))
        return;
    id = c->elements[index].id;
    hit = fuse_internal_last_hit(c, id);
    if (c->debug && c->capture_id == id)
        c->capture_id = 0;
    if (c->capture_id == id) {
        if (c->pointer_state != FUSE_POINTER_PRESSED)
            c->capture_id = 0;
    } else if (c->capture_id == 0 && hit && c->pointer_edge == FUSE_EDGE_PRESSED) {
        c->capture_id = id;
    }
    if (c->input_enabled && c->capture_id == id) {
        item = fuse_internal_last_box(c, id);
        if (item && item->w > 0.0f)
            *nob_pos = fuse_internal_clamp01((c->pointer_x - item->x) / item->w);
    }
    *nob_pos = fuse_internal_clamp01(*nob_pos);
    c->elements[index].color = track;
    c->elements[index].color_alt = nob;
    c->elements[index].nob_pos = *nob_pos;
}

static int
fuse_utf8_prev(const char *s, int i)
{
    if (!s || i <= 0)
        return 0;
    i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
        i--;
    return i;
}

static int
fuse_utf8_next(const char *s, int i)
{
    int len;

    len = 0;
    if (!s)
        return 0;
    while (s[len])
        len++;
    if (i < 0 || i >= len)
        return len;
    i++;
    while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80)
        i++;
    return i;
}

static int
fuse_text_len(const char *s)
{
    int n;

    n = 0;
    if (!s)
        return 0;
    while (s[n])
        n++;
    return n;
}

static int
fuse_text_insert(char *buf, int cap, int *caret, const char *s, int n)
{
    int len;

    if (!buf || !caret || !s || n <= 0 || cap < 2)
        return 0;
    len = fuse_text_len(buf);
    if (*caret < 0 || *caret > len)
        *caret = len;
    if (len + n >= cap)
        n = cap - 1 - len;
    if (n <= 0)
        return 0;
    memmove(buf + *caret + n, buf + *caret, (size_t)(len - *caret + 1));
    memcpy(buf + *caret, s, (size_t)n);
    *caret += n;
    return 1;
}

static int
fuse_byte_ok(int (*allow)(unsigned char ch, void *user), void *user, unsigned char ch)
{
    if (allow)
        return allow(ch, user) ? 1 : 0;
    return ch >= 32 && ch < 127;
}

static int
fuse_textbox_apply(FuseCanvas c, char *buf, int cap, int *caret,
    int (*allow)(unsigned char ch, void *user), void *user)
{
    int changed;
    uint16_t i;

    changed = 0;
    if (!c || !buf || !caret)
        return 0;
    for (i = 0; i < c->inp_n; i++) {
        FuseInp *ev;
        int len;

        ev = &c->inp[i];
        len = fuse_text_len(buf);
        if (*caret < 0 || *caret > len)
            *caret = len;
        if (ev->kind == FUSE_INP_KEY) {
            if (ev->key == FUSE_KEY_BACKSPACE) {
                int prev;

                prev = fuse_utf8_prev(buf, *caret);
                if (prev != *caret) {
                    memmove(buf + prev, buf + *caret, (size_t)(len - *caret + 1));
                    *caret = prev;
                    changed = 1;
                }
            } else if (ev->key == FUSE_KEY_DELETE) {
                int next;

                next = fuse_utf8_next(buf, *caret);
                if (next != *caret) {
                    memmove(buf + *caret, buf + next, (size_t)(len - next + 1));
                    changed = 1;
                }
            } else if (ev->key == FUSE_KEY_LEFT) {
                *caret = fuse_utf8_prev(buf, *caret);
            } else if (ev->key == FUSE_KEY_RIGHT) {
                *caret = fuse_utf8_next(buf, *caret);
            } else if (ev->key == FUSE_KEY_HOME) {
                *caret = 0;
            } else if (ev->key == FUSE_KEY_END) {
                *caret = len;
            }
        } else if (ev->kind == FUSE_INP_TEXT && ev->n > 0) {
            char clean[64];
            int o, k;

            o = 0;
            for (k = 0; k < (int)ev->n && o + 1 < (int)sizeof clean; k++) {
                unsigned char ch;

                ch = (unsigned char)c->inp_bytes[ev->off + (uint16_t)k];
                if (!fuse_byte_ok(allow, user, ch))
                    continue;
                clean[o++] = (char)ch;
            }
            if (o > 0 && fuse_text_insert(buf, cap, caret, clean, o))
                changed = 1;
        }
    }
    fuse_input_clear(c);
    return changed;
}

bool
fuse_textbox(FuseCanvas c, float x, float y, float w, float h, float pad_l, float pad_r,
    char *buf, int cap, int *caret, const char *placeholder, float size,
    uint32_t fill, uint32_t hovered, uint32_t ink, uint32_t ghost, int caret_on,
    int (*allow)(unsigned char ch, void *user), void *user)
{
    uint32_t index, id;
    int hit;
    int changed;
    int len;

    FASSERT(c, "null canvas");
    FASSERT(buf, "null textbox buf");
    FASSERT(caret, "null textbox caret");
    changed = 0;
    if (!fuse_internal_ok(c) || !buf || !caret || cap < 2)
        return false;
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_FIELD);
    if (!fuse_internal_ok(c))
        return false;
    id = c->elements[index].id;
    hit = fuse_internal_last_hit(c, id);
    if (id == c->focus_frame && id != 0)
        changed = fuse_textbox_apply(c, buf, cap, caret, allow, user);
    len = fuse_text_len(buf);
    if (hit && c->pointer_edge == FUSE_EDGE_RELEASED && c->capture_id == 0) {
        c->focus_id = id;
        if (*caret < 0 || *caret > len)
            *caret = len;
        *caret = len;
    } else if (id == c->focus_id && (*caret < 0 || *caret > len)) {
        *caret = len;
    }
    if (c->field_n >= FUSE_FIELD_MAX) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return changed ? true : false;
    }
    {
        FuseFieldRun *run;
        int slot;
        float pl, pr;

        slot = (int)c->field_n;
        c->field_n++;
        run = &c->fields[slot];
        memset(run, 0, sizeof *run);
        run->str = buf;
        run->color = ink;
        run->caret_color = ink;
        run->size = size > 0.0f ? size : 16.0f;
        run->caret = -1;
        if (!buf[0] && placeholder && placeholder[0]) {
            run->str = placeholder;
            run->color = ghost;
        }
        if (id != 0 && id == c->focus_id && caret_on)
            run->caret = *caret;
        pl = (float)fuse_px_i(pad_l);
        pr = (float)fuse_px_i(pad_r);
        if (pl < 0.0f)
            pl = 0.0f;
        if (pr < 0.0f)
            pr = 0.0f;
        c->elements[index].color = hit ? hovered : fill;
        c->elements[index].color_alt = ink;
        /* Pads live in scroll/nob_pos. last_child is the run slot plus one.
         * A field has no child elements, so those slots are free. */
        c->elements[index].scroll = pl;
        c->elements[index].nob_pos = pr;
        c->elements[index].last_child = (uint32_t)slot + 1;
    }
    return changed ? true : false;
}

const FuseFieldRun *
fuse_canvas_fields(FuseCanvas c, size_t *count)
{
    if (count)
        *count = c ? c->field_n : 0;
    if (!c)
        return NULL;
    return c->fields;
}

bool
fuse_textbox_edit(FuseCanvas c, const char *name, char *buf, int cap, int *caret,
    int (*allow)(unsigned char ch, void *user), void *user)
{
    uint32_t id;

    FASSERT(c, "null canvas");
    FASSERT(name, "null textbox name");
    if (!c || !name || !name[0] || !buf || !caret || cap < 2)
        return false;
    id = fuse_internal_scoped_id(c, fuse_internal_hash_str(name));
    if (id == 0 || id != c->focus_frame)
        return false;
    return fuse_textbox_apply(c, buf, cap, caret, allow, user) ? true : false;
}

static float
fuse_internal_percent(FuseCanvas c, float p, int yaxis)
{
    FuseElement *el;
    if (!c || c->screen_count == 0)
        return 0.0f;
    el = &c->elements[c->screens[c->screen_count - 1].element];
    return p * (yaxis ? el->h : el->w) / 100.0f;
}

float
fuse_percent_x(FuseCanvas c, float p)
{
    FASSERT(c, "null canvas");
    return fuse_internal_percent(c, p, 0);
}

float
fuse_percent_y(FuseCanvas c, float p)
{
    FASSERT(c, "null canvas");
    return fuse_internal_percent(c, p, 1);
}

static uint32_t
fuse_internal_open_maybe_anon(FuseCanvas c, float x, float y, float w, float h, uint16_t flags)
{
    uint32_t parent_el, id, i;

    parent_el = c->screens[c->screen_count - 1].element;
    id = c->pending_id;
    c->pending_id = 0;
    if (id != 0) {
        for (i = 0; i < c->element_count; i++) {
            if (c->elements[i].id == id) {
                c->pending_name[0] = 0;
                fuse_internal_drop_sizing(c);
                fuse_internal_fail(c, FUSE_ERR_DUPLICATE_ID);
                return 0;
            }
        }
    }
    return fuse_internal_add_element(c, parent_el, id, x, y, w, h, flags);
}

static uint64_t
fuse_internal_glyph(int c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 0 || c > 127)
        return 0;
    return fuse_font[c];
}

void
fuse_rect(FuseCanvas c, float x, float y, float w, float h, uint32_t color)
{
    uint32_t index;

    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    index = fuse_internal_open_maybe_anon(c, x, y, w, h, FUSE_EL_RECT);
    if (!fuse_internal_ok(c))
        return;
    c->elements[index].color = color;
}

int
fuse_radial_pick(float cx, float cy, float inner, float outer, int count, float px, float py)
{
    float dx, dy, radius, angle, span;
    int slice;

    if (count < 1 || count > 8 || !isfinite(cx) || !isfinite(cy) ||
        !isfinite(inner) || !isfinite(outer) || !isfinite(px) || !isfinite(py) ||
        inner < 0.0f || outer <= inner)
        return -1;
    dx = px - cx;
    dy = py - cy;
    radius = hypotf(dx, dy);
    if (!(radius > inner && radius < outer))
        return -1;
    span = 2.0f * FUSE_PI / count;
    angle = atan2f(dy, dx) + span * 0.5f;
    if (angle < 0.0f)
        angle += 2.0f * FUSE_PI;
    slice = (int)(angle / span);
    return slice >= count ? 0 : slice;
}

int
fuse_radial(FuseCanvas c, float cx, float cy, float inner, float outer, int count,
    float px, float py, uint32_t idle, uint32_t hovered)
{
    uint32_t index;
    FuseElement *el;
    int pick = fuse_radial_pick(cx, cy, inner, outer, count, px, py);

    if (!fuse_internal_ok(c) || count < 1 || count > 8 ||
        !isfinite(cx) || !isfinite(cy) || !isfinite(inner) || !isfinite(outer) ||
        inner < 0.0f || outer <= inner)
        return -1;
    index = fuse_internal_open_maybe_anon(c, cx - outer, cy - outer,
        outer * 2.0f, outer * 2.0f, FUSE_EL_RADIAL);
    if (!fuse_internal_ok(c))
        return -1;
    el = &c->elements[index];
    /* Preserve unsnapped geometry. These slots are unused by this leaf:
     * nob_pos = inner radius, scroll = hover, child_count = slice count. */
    el->x = cx - outer; el->y = cy - outer;
    el->w = el->h = outer * 2.0f;
    el->nob_pos = inner;
    el->scroll = (float)pick;
    el->child_count = (uint32_t)count;
    el->color = idle;
    el->color_alt = hovered;
    return pick;
}

void
fuse_image(FuseCanvas c, float x, float y, float w, float h, uint32_t handle)
{
    uint32_t index;

    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    if (w == 0.0f || h == 0.0f) {
        c->pending_id = 0;
        c->pending_name[0] = 0;
        fuse_internal_drop_sizing(c);
        return;
    }
    index = fuse_internal_open_maybe_anon(c, x, y, w, h, FUSE_EL_IMAGE);
    if (!fuse_internal_ok(c))
        return;
    c->elements[index].color = handle;
}

float
fuse_text_width(const char *str, float s)
{
    int n;

    if (!str)
        return 0.0f;
    n = 0;
    while (str[n])
        n++;
    if (n == 0)
        return 0.0f;
    s = fuse_px_scale(s);
    return (float)n * 6.0f * s - s;
}

void
fuse_text(FuseCanvas c, float x, float y, float s, const char *str, uint32_t color)
{
    float cx;
    float tw;
    uint32_t index;

    FASSERT(c, "null canvas");
    FASSERT(str, "null text");
    if (!fuse_internal_ok(c) || !str)
        return;
    if (!str[0]) {
        c->pending_id = 0;
        c->pending_name[0] = 0;
        fuse_internal_drop_sizing(c);
        return;
    }
    s = fuse_px_scale(s);
    x = (float)fuse_px_i(x);
    y = (float)fuse_px_i(y);
    tw = fuse_text_width(str, s);
    if (tw < 1.0f)
        tw = 1.0f;
    if (c->screen_count >= c->screen_cap) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return;
    }
    index = fuse_internal_open_el(c, x, y, tw, 7.0f * s, FUSE_EL_TEXT);
    if (!fuse_internal_ok(c) || index == 0)
        return;
    c->screens[c->screen_count].element = index;
    c->screen_count++;
    cx = 0.0f;
    while (*str) {
        uint64_t g;
        int row;

        g = fuse_internal_glyph((unsigned char)*str);
        for (row = 0; row < 7; row++) {
            uint32_t bits;
            int col;

            bits = (uint32_t)((g >> (row * 5)) & 0x1F);
            col = 0;
            while (col < 5) {
                if (bits & (1u << (4 - col))) {
                    int start;

                    start = col;
                    while (col < 5 && (bits & (1u << (4 - col))))
                        col++;
                    fuse_rect(c, cx + (float)start * s, (float)row * s,
                              (float)(col - start) * s, s, color);
                } else {
                    col++;
                }
            }
        }
        cx += 6.0f * s;
        str++;
    }
    c->screen_count--;
}

void
fuse_sizing(FuseCanvas c, uint8_t width_sizing, uint8_t height_sizing)
{
    FASSERT(c, "null canvas");
    if (!c || !fuse_internal_ok(c))
        return;
    if (width_sizing > FUSE_SIZING_PERCENT)
        width_sizing = FUSE_SIZING_FIT;
    if (height_sizing > FUSE_SIZING_PERCENT)
        height_sizing = FUSE_SIZING_FIT;
    c->pending_sizing = 1;
    c->pending_w_sizing = width_sizing;
    c->pending_h_sizing = height_sizing;
}

void
fuse_space(FuseCanvas c)
{
    uint32_t parent;
    const FuseClass *cls;
    int vertical;

    FASSERT(c, "null canvas");
    if (!c || !fuse_internal_ok(c) || c->screen_count == 0)
        return;
    parent = c->screens[c->screen_count - 1].element;
    cls = c->elements[parent].cls;
    vertical = 0;
    if (c->elements[parent].flags & FUSE_EL_COL)
        vertical = 1;
    else if (cls && cls->direction == FUSE_DIRECTION_COLUMN)
        vertical = 1;
    if (vertical)
        fuse_sizing(c, FUSE_SIZING_FIT, FUSE_SIZING_GROW);
    else
        fuse_sizing(c, FUSE_SIZING_GROW, FUSE_SIZING_FIT);
    fuse_internal_open_el(c, 0.0f, 0.0f, 0.0f, 0.0f, FUSE_EL_SPACE);
}

bool
fuse_button_text(FuseCanvas c, char *text, float x, float y, float w, float h, uint32_t selected, uint32_t hovered)
{
    bool clicked;
    float s;
    float tw;
    uint32_t parent;
    uint32_t index;

    fuse_px_box(&x, &y, &w, &h);
    clicked = fuse_button(c, x, y, w, h, selected, hovered);
    if (!text || !text[0] || !fuse_internal_ok(c))
        return clicked;
    parent = c->screens[c->screen_count - 1].element;
    index = c->elements[parent].last_child;
    if (index == 0 || c->screen_count >= c->screen_cap)
        return clicked;
    c->screens[c->screen_count].element = index;
    c->screen_count++;
    s = h / 12.0f;
    if (s < 2.0f)
        s = 2.0f;
    s = fuse_px_scale(s);
    tw = fuse_text_width(text, s);
    fuse_text(c, (float)(((int)(w - tw)) / 2), (float)(((int)(h - 7.0f * s)) / 2), s, text, 0xFF000000u);
    c->screen_count--;
    return clicked;
}

void
fuse_canvas_layer(FuseCanvas c, int16_t layer)
{
	if (!c) return;
	if (c->layer_count && c->layer_elements[c->layer_count - 1] == c->element_count) {
		c->layers[c->layer_count - 1] = layer;
		return;
	}
	if (c->layer_count == 64) {
		fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
		return;
	}
	c->layer_elements[c->layer_count] = c->element_count;
	c->layers[c->layer_count++] = layer;
}

void
fuse_window_begin(FuseCanvas c, char *name, float x, float y, float w, float h, const FuseClass *cls)
{
	fuse_id(c, name);
	fuse_div_begin(c, x, y, w, h, cls);
}

void
fuse_window_begin_scroll(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls, float *scroll)
{
	fuse_div_begin_scroll(c, x, y, w, h, cls, scroll);
}

void
fuse_window_end(FuseCanvas c)
{
	fuse_div_end(c);
}

void
fuse_canvas_input_enabled(FuseCanvas c, int enabled)
{
	if (c) c->input_enabled = enabled != 0;
}

int16_t
fuse_internal_layer(FuseCanvas c, uint32_t index)
{
	uint32_t i;
	int16_t layer = 0;
	for (i = 0; i < c->layer_count && c->layer_elements[i] <= index; i++)
		layer = c->layers[i];
	return layer;
}

/* Docking implementation. */
/* Copyright (c) 2026 Vasco Alves. No heap allocation; bounded traversal. */
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct FuseDockWindow {
	char name[32], title[64];
	float min_w, min_h;
} FuseDockWindow;
struct FuseDock {
	int capacity, count, revision, focused, tab_focus;
	int rollback_focus, rollback_tab_focus;
	int capture, window, node, dragging, side, target;
	float start_x, start_y;
	FuseDockRect host, start_box, boxes[FUSE_DOCK_NODES];
	FuseDockLayout layout, rollback;
	FuseDockWindow windows[FUSE_DOCK_MAX];
};

static int fuse_dock_contains(FuseDockRect r, float x, float y);
static int fuse_dock_group(const FuseDock *d, int window);
static int fuse_dock_slot(FuseDock *d);
static int fuse_dock_top_float(const FuseDock *d, float x, float y);
static int fuse_dock_can_split(const FuseDock *d, int window, int group, FuseDockSide side);
static void fuse_dock_cancel(FuseDock *d);
static int fuse_dock_float_root(const FuseDock *d, int node);
static void fuse_dock_remove(FuseDock *d, int window);
static void fuse_dock_min(const FuseDock *d, int node, float *w, float *h);
static void fuse_dock_resolve(FuseDock *d, int node, FuseDockRect r);
static void fuse_dock_raise(FuseDock *d, int node);
static FuseDockSide fuse_dock_target(const FuseDock *d, int node, float x, float y);
static int fuse_dock_check(const FuseDock *d, const FuseDockLayout *l, int node, int floating, unsigned *seen, unsigned *windows);

size_t
fuse_dock_memory(void)
{
	return sizeof(FuseDock);
}

FuseDock *
fuse_dock_create(void *memory, size_t size, int capacity)
{
	FuseDock *d = memory;
	if (!d || (uintptr_t)d % 8 || size < sizeof(*d) || capacity < 1 || capacity > FUSE_DOCK_MAX)
		return NULL;
	memset(d, 0, sizeof(*d));
	d->capacity = capacity;
	d->layout.root = -1;
	d->focused = -1;
	return d;
}

int
fuse_dock_register(FuseDock *d, const char *name, const char *title, float w, float h)
{
	int i;
	if (!d || d->capture || !name || !title || !name[0] || strlen(name) >= 32 || strlen(title) >= 64 ||
		!isfinite(w) || !isfinite(h) || w < 0 || h < 0 || d->count == d->capacity)
		return -1;
	for (i = 0; i < d->count; i++)
		if (!strcmp(name, d->windows[i].name))
			return -1;
	i = d->count++;
	strcpy(d->windows[i].name, name);
	strcpy(d->windows[i].title, title);
	d->windows[i].min_w = w;
	d->windows[i].min_h = h;
	fuse_dock_place(d, i, -1, FUSE_DOCK_CENTER);
	return i;
}

const char *
fuse_dock_name(const FuseDock *d, int window)
{
	return window >= 0 && window < d->count ? d->windows[window].name : NULL;
}

const char *
fuse_dock_title(const FuseDock *d, int window)
{
	return window >= 0 && window < d->count ? d->windows[window].title : NULL;
}

int
fuse_dock_contains(FuseDockRect r, float x, float y)
{
	return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

int
fuse_dock_group(const FuseDock *d, int window)
{
	int i, j;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind == 1)
			for (j = 0; j < d->layout.nodes[i].count; j++)
				if (d->layout.nodes[i].tabs[j] == window)
					return i;
	return -1;
}

int
fuse_dock_float_root(const FuseDock *d, int node)
{
	int i, depth;
	for (depth = 0; node >= 0 && depth < FUSE_DOCK_NODES; depth++) {
		if (d->layout.nodes[node].floating) return node;
		for (i = 0; i < FUSE_DOCK_NODES; i++)
			if (d->layout.nodes[i].kind >= 2 && (d->layout.nodes[i].a == node || d->layout.nodes[i].b == node)) break;
		if (i == FUSE_DOCK_NODES) return -1;
		node = i;
	}
	return -1;
}

/* A floating split's gaps occlude docked panels and splitters too. */
int
fuse_dock_top_float(const FuseDock *d, float x, float y)
{
	int i, top = -1;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind && d->layout.nodes[i].floating &&
			fuse_dock_contains(d->boxes[i], x, y) &&
			(top < 0 || d->layout.nodes[i].order > d->layout.nodes[top].order))
			top = i;
	return top;
}

int
fuse_dock_can_split(const FuseDock *d, int window, int group, FuseDockSide side)
{
	float mw, mh, tw = 0, th = 0;
	int i;
	const FuseDockNode *n;
	if (group < 0 || side == FUSE_DOCK_CENTER || side == FUSE_DOCK_FLOAT || (d->host.w == 0 && d->host.h == 0))
		return 1;
	n = &d->layout.nodes[group];
	/* The dragged tab leaves this group: do not count its minimum twice. */
	for (i = 0; i < n->count; i++) {
		if (n->tabs[i] == window) continue;
		tw = fmaxf(tw, d->windows[n->tabs[i]].min_w);
		th = fmaxf(th, d->windows[n->tabs[i]].min_h + FUSE_DOCK_TITLE);
	}
	mw = d->windows[window].min_w;
	mh = d->windows[window].min_h + FUSE_DOCK_TITLE;
	return d->boxes[group].w >= (side <= FUSE_DOCK_RIGHT ? mw + tw + 4 : fmaxf(mw, tw)) &&
		d->boxes[group].h >= (side >= FUSE_DOCK_TOP ? mh + th + 4 : fmaxf(mh, th));
}

void
fuse_dock_cancel(FuseDock *d)
{
	if (!d->capture) return;
	d->layout = d->rollback;
	d->focused = d->rollback_focus;
	d->tab_focus = d->rollback_tab_focus;
	d->capture = 0;
	d->revision++;
}

int
fuse_dock_slot(FuseDock *d)
{
	int i;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (!d->layout.nodes[i].kind)
			return i;
	return -1;
}

void
fuse_dock_remove(FuseDock *d, int window)
{
	int i, j, group = fuse_dock_group(d, window);
	FuseDockNode *n;
	if (group < 0)
		return;
	n = &d->layout.nodes[group];
	for (j = 0; j < n->count && n->tabs[j] != window; j++) {}
	memmove(n->tabs + j, n->tabs + j + 1, (size_t)(n->count - j - 1) * sizeof(int));
	if (--n->count) {
		if (j < n->active)
			n->active--;
		else if (n->active >= n->count)
			n->active = n->count - 1;
		return;
	}
	n->kind = 0;
	if (d->layout.root == group)
		d->layout.root = -1;
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		n = &d->layout.nodes[i];
		if (n->kind < 2 || (n->a != group && n->b != group))
			continue;
		/* Copy sibling into parent; indexed parent references stay valid. */
		j = n->a == group ? n->b : n->a;
		{
			int floating = n->floating, order = n->order;
			FuseDockRect rect = n->rect;
			*n = d->layout.nodes[j];
			n->floating = floating;
			if (floating) { n->rect = rect; n->order = order; }
		}
		d->layout.nodes[j].kind = 0;
		break;
	}
}

void
fuse_dock_reset(FuseDock *d)
{
	int i;
	memset(&d->layout, 0, sizeof(d->layout));
	d->layout.root = -1;
	d->capture = 0;
	d->focused = -1;
	d->tab_focus = 0;
	d->revision++;
	for (i = 0; i < d->count; i++)
		fuse_dock_place(d, i, -1, FUSE_DOCK_CENTER);
}

int
fuse_dock_place(FuseDock *d, int window, int target, FuseDockSide side)
{
	FuseDockLayout before;
	FuseDockNode *n;
	int group, source, slot, child;
	if (!d || d->capture || window < 0 || window >= d->count || side < 0 || side > FUSE_DOCK_FLOAT)
		return 0;
	if (side == FUSE_DOCK_FLOAT) target = -1;
	group = fuse_dock_group(d, target);
	source = fuse_dock_group(d, window);
	if (target >= 0 && group < 0)
		return 0;
	if (source == group && source >= 0 && (d->layout.nodes[source].count == 1 ||
		(side == FUSE_DOCK_CENTER && target == window))) {
		n = &d->layout.nodes[source];
		for (slot = 0; n->tabs[slot] != window; slot++) {}
		if (n->active != slot) { n->active = slot; d->revision++; }
		d->focused = window;
		return 1;
	}
	if (!fuse_dock_can_split(d, window, group, side))
		return 0;
	if (target == window && group >= 0 && d->layout.nodes[group].count > 1)
		target = d->layout.nodes[group].tabs[d->layout.nodes[group].tabs[0] == window ? 1 : 0];
	before = d->layout;
	fuse_dock_remove(d, window);
	/* A collapsing source can relocate the target group. */
	group = fuse_dock_group(d, target);
	if (side == FUSE_DOCK_CENTER && group < 0 && d->layout.root >= 0)
		group = fuse_dock_group(d, fuse_dock_visible(d, 0));
	if (side == FUSE_DOCK_CENTER && group >= 0) {
		n = &d->layout.nodes[group];
		n->tabs[n->count++] = window;
		n->active = n->count - 1;
	} else {
		if ((slot = fuse_dock_slot(d)) < 0)
			goto fail;
		n = &d->layout.nodes[slot];
		memset(n, 0, sizeof(*n));
		n->kind = 1;
		n->count = 1;
		n->tabs[0] = window;
		if (side == FUSE_DOCK_FLOAT || (group < 0 && d->layout.root >= 0)) {
			n->floating = 1;
			n->rect = (FuseDockRect){0.1f, 0.1f, 0.5f, 0.5f};
			fuse_dock_raise(d, slot);
		} else if (group < 0) {
			d->layout.root = slot;
		} else {
			if ((child = fuse_dock_slot(d)) < 0)
				goto fail;
			d->layout.nodes[child] = d->layout.nodes[group];
			d->layout.nodes[child].floating = 0;
			n = &d->layout.nodes[group];
			{
				int floating = n->floating, order = n->order;
				FuseDockRect rect = n->rect;
				memset(n, 0, sizeof(*n));
				n->floating = floating; n->order = order; n->rect = rect;
			}
			n->kind = side <= FUSE_DOCK_RIGHT ? 2 : 3;
			n->ratio = 0.5f;
			n->a = side == FUSE_DOCK_LEFT || side == FUSE_DOCK_TOP ? slot : child;
			n->b = n->a == slot ? child : slot;
		}
	}
	d->focused = window;
	if (memcmp(&before, &d->layout, sizeof(before))) d->revision++;
	fuse_dock_layout(d, d->host);
	return 1;
fail:
	d->layout = before;
	return 0;
}

void
fuse_dock_min(const FuseDock *d, int node, float *w, float *h)
{
	const FuseDockNode *n = &d->layout.nodes[node];
	float aw, ah, bw, bh;
	int i;
	*w = *h = 0;
	if (n->kind == 1) {
		for (i = 0; i < n->count; i++) {
			*w = fmaxf(*w, d->windows[n->tabs[i]].min_w);
			*h = fmaxf(*h, d->windows[n->tabs[i]].min_h + FUSE_DOCK_TITLE);
		}
		return;
	}
	fuse_dock_min(d, n->a, &aw, &ah);
	fuse_dock_min(d, n->b, &bw, &bh);
	*w = n->kind == 2 ? aw + bw + 4 : fmaxf(aw, bw);
	*h = n->kind == 3 ? ah + bh + 4 : fmaxf(ah, bh);
}

void
fuse_dock_resolve(FuseDock *d, int node, FuseDockRect r)
{
	FuseDockNode *n = &d->layout.nodes[node];
	FuseDockRect a = r, b = r;
	float aw, ah, bw, bh, length, cut, gap;
	d->boxes[node] = r;
	if (n->kind == 1)
		return;
	fuse_dock_min(d, n->a, &aw, &ah);
	fuse_dock_min(d, n->b, &bw, &bh);
	gap = fminf(4, n->kind == 2 ? r.w : r.h);
	length = fmaxf(0, (n->kind == 2 ? r.w : r.h) - gap);
	cut = length * n->ratio;
	if (length >= (n->kind == 2 ? aw + bw : ah + bh))
		cut = fmaxf(n->kind == 2 ? aw : ah, fminf(cut, length - (n->kind == 2 ? bw : bh)));
	if (n->kind == 2) {
		a.w = cut;
		b.x += cut + gap;
		b.w = length - cut;
	} else {
		a.h = cut;
		b.y += cut + gap;
		b.h = length - cut;
	}
	fuse_dock_resolve(d, n->a, a);
	fuse_dock_resolve(d, n->b, b);
}

void
fuse_dock_layout(FuseDock *d, FuseDockRect host)
{
	FuseDockNode *n;
	FuseDockRect r;
	float w, h;
	int i;
	if (!d || !isfinite(host.x) || !isfinite(host.y) || !isfinite(host.w) || !isfinite(host.h) || !isfinite(host.x + host.w) || !isfinite(host.y + host.h) || host.w < 0 || host.h < 0)
		return;
	if (d->capture && (host.x != d->host.x || host.y != d->host.y || host.w != d->host.w || host.h != d->host.h))
		fuse_dock_cancel(d);
	d->host = host;
	memset(d->boxes, 0, sizeof(d->boxes));
	if (d->layout.root >= 0)
		fuse_dock_resolve(d, d->layout.root, host);
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		n = &d->layout.nodes[i];
		if (!n->kind || !n->floating)
			continue;
		fuse_dock_min(d, i, &w, &h);
		r.w = fminf(host.w, fmaxf(w, host.w * n->rect.w));
		r.h = fminf(host.h, fmaxf(h, host.h * n->rect.h));
		r.x = host.x + fmaxf(0, fminf(host.w - r.w, host.w * n->rect.x));
		r.y = host.y + fmaxf(0, fminf(host.h - r.h, host.h * n->rect.y));
		fuse_dock_resolve(d, i, r);
	}
}

void
fuse_dock_raise(FuseDock *d, int node)
{
	int i, old = d->layout.nodes[node].order;
	if (old == FUSE_DOCK_MAX) return;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind && d->layout.nodes[i].floating && d->layout.nodes[i].order > old)
			d->layout.nodes[i].order--;
	d->layout.nodes[node].order = FUSE_DOCK_MAX;
	/* Renormalize to avoid unbounded counters over the workspace lifetime. */
}

int
fuse_dock_visible(const FuseDock *d, int position)
{
	int i, order;
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (d->layout.nodes[i].kind == 1 && fuse_dock_float_root(d, i) < 0)
			if (!position--)
				return d->layout.nodes[i].tabs[d->layout.nodes[i].active];
	for (order = 0; order <= FUSE_DOCK_MAX; order++)
		for (i = 0; i < FUSE_DOCK_NODES; i++)
			if (d->layout.nodes[i].kind == 1 && fuse_dock_float_root(d, i) >= 0 && d->layout.nodes[fuse_dock_float_root(d, i)].order == order)
				if (!position--)
					return d->layout.nodes[i].tabs[d->layout.nodes[i].active];
	return -1;
}

int
fuse_dock_hit(const FuseDock *d, float x, float y)
{
	int i, window, hit = -1, group, top;
	if (!fuse_dock_contains(d->host, x, y)) return -1;
	top = fuse_dock_top_float(d, x, y);
	for (i = 0; (window = fuse_dock_visible(d, i)) >= 0; i++) {
		group = fuse_dock_group(d, window);
		if (fuse_dock_float_root(d, group) == top && fuse_dock_contains(d->boxes[group], x, y))
			hit = window;
	}
	return hit;
}

int
fuse_dock_group_rect(const FuseDock *d, int window, FuseDockRect *r)
{
	int group = fuse_dock_group(d, window);
	if (group < 0 || d->layout.nodes[group].tabs[d->layout.nodes[group].active] != window)
		return 0;
	*r = d->boxes[group];
	return 1;
}

int
fuse_dock_content(const FuseDock *d, int window, FuseDockRect *r)
{
	if (!fuse_dock_group_rect(d, window, r))
		return 0;
	r->y += fminf(FUSE_DOCK_TITLE, r->h);
	r->h = fmaxf(0, r->h - FUSE_DOCK_TITLE);
	return r->w > 0 && r->h > 0;
}

FuseDockSide
fuse_dock_target(const FuseDock *d, int node, float x, float y)
{
	FuseDockRect r = d->boxes[node];
	float nx = (x - r.x) / fmaxf(1, r.w), ny = (y - r.y) / fmaxf(1, r.h);
	if (ny < 0.2f) return FUSE_DOCK_TOP;
	if (ny > 0.8f) return FUSE_DOCK_BOTTOM;
	if (nx < 0.2f) return FUSE_DOCK_LEFT;
	if (nx > 0.8f) return FUSE_DOCK_RIGHT;
	return FUSE_DOCK_CENTER;
}

int
fuse_dock_event(FuseDock *d, const FuseDockEvent *e)
{
	FuseDockNode *n;
	FuseDockRect r, a;
	FuseDockLayout before;
	int i, hit, group, tab, floating, top, focus_before, tabs_before;
	float dx, dy, aw, ah, bw, bh, length, cut, grip;
	if (!d || !e)
		return 0;
	if (e->cancel) {
		i = d->capture != 0;
		fuse_dock_cancel(d);
		fuse_dock_layout(d, d->host);
		return i;
	}
	if (!isfinite(e->x) || !isfinite(e->y)) {
		i = d->capture != 0;
		fuse_dock_cancel(d);
		fuse_dock_layout(d, d->host);
		return i;
	}
	if (d->capture && (e->previous || e->next || e->activate)) return 1;
	if (!d->capture && (e->previous || e->next || e->activate)) {
		if (!d->tab_focus) return 0;
		group = fuse_dock_group(d, d->focused);
		if (group < 0) return 0;
		n = &d->layout.nodes[group];
		i = n->active;
		n->active = (n->active + n->count + (e->previous ? -1 : e->next ? 1 : 0)) % n->count;
		d->focused = n->tabs[n->active];
		if (i != n->active) d->revision++;
		return 1;
	}
	hit = fuse_dock_hit(d, e->x, e->y);
	group = fuse_dock_group(d, hit);
	top = fuse_dock_top_float(d, e->x, e->y);
	if (d->capture) {
		dx = e->x - d->start_x;
		dy = e->y - d->start_y;
		d->dragging |= dx * dx + dy * dy >= 25;
		n = &d->layout.nodes[d->node];
		if (d->capture == 1) {
			d->target = hit;
			d->side = group >= 0 ? fuse_dock_target(d, group, e->x, e->y) : FUSE_DOCK_FLOAT;
		} else if (d->dragging) {
			r = d->start_box;
			if (d->capture == 2 || d->capture == 3) {
				n->rect = (FuseDockRect){(r.x - d->host.x) / fmaxf(1, d->host.w),
					(r.y - d->host.y) / fmaxf(1, d->host.h), r.w / fmaxf(1, d->host.w), r.h / fmaxf(1, d->host.h)};
				if (d->capture == 2) {
					n->rect.x += dx / fmaxf(1, d->host.w);
					n->rect.y += dy / fmaxf(1, d->host.h);
				} else {
					n->rect.w = fmaxf(0.01f, n->rect.w + dx / fmaxf(1, d->host.w));
					n->rect.h = fmaxf(0.01f, n->rect.h + dy / fmaxf(1, d->host.h));
				}
			} else {
				fuse_dock_min(d, n->a, &aw, &ah);
				fuse_dock_min(d, n->b, &bw, &bh);
				length = fmaxf(0, (n->kind == 2 ? r.w : r.h) - 4);
				cut = n->kind == 2 ? e->x - r.x : e->y - r.y;
				if (length >= (n->kind == 2 ? aw + bw : ah + bh))
					cut = fmaxf(n->kind == 2 ? aw : ah, fminf(cut, length - (n->kind == 2 ? bw : bh)));
				n->ratio = fmaxf(0.01f, fminf(0.99f, cut / fmaxf(1, length)));
			}
			fuse_dock_layout(d, d->host);
		}
		if (e->release) {
			if (!fuse_dock_contains(d->host, e->x, e->y))
				fuse_dock_cancel(d);
			else if (d->dragging && d->capture == 1) {
				if (group == d->node && e->y < d->boxes[group].y + FUSE_DOCK_TITLE) {
					n = &d->layout.nodes[group];
					tab = (int)((e->x - d->boxes[group].x) / fmaxf(1, (d->boxes[group].w - (fuse_dock_float_root(d, group) >= 0 ? fminf(24, d->boxes[group].w * 0.25f) : 0)) / n->count));
					tab = tab < 0 ? 0 : tab >= n->count ? n->count - 1 : tab;
					for (i = 0; n->tabs[i] != d->window; i++) {}
					if (i != tab) d->revision++;
					while (i < tab) { n->tabs[i] = n->tabs[i + 1]; i++; }
					while (i > tab) { n->tabs[i] = n->tabs[i - 1]; i--; }
					n->tabs[tab] = d->window;
					n->active = tab;
				} else {
					d->capture = 0;
					if (!fuse_dock_place(d, d->window, d->target, d->side)) {
						d->capture = 1;
						fuse_dock_cancel(d);
					} else if (d->side == FUSE_DOCK_FLOAT) {
						n = &d->layout.nodes[fuse_dock_group(d, d->window)];
						n->rect.x = (e->x - d->host.x) / fmaxf(1, d->host.w);
						n->rect.y = (e->y - d->host.y) / fmaxf(1, d->host.h);
					}
				}
			} else if (d->dragging) {
				if (d->capture == 2 || d->capture == 3) {
					r = d->boxes[d->node];
					n->rect = (FuseDockRect){(r.x - d->host.x) / fmaxf(1, d->host.w),
						(r.y - d->host.y) / fmaxf(1, d->host.h), r.w / fmaxf(1, d->host.w), r.h / fmaxf(1, d->host.h)};
				}
				d->revision++;
			}
			d->capture = 0;
			fuse_dock_layout(d, d->host);
		}
		return 1;
	}
	if (!e->press)
		return (group >= 0 && e->y < d->boxes[group].y + FUSE_DOCK_TITLE) || (top >= 0 && group < 0);
	if (!fuse_dock_contains(d->host, e->x, e->y)) return 0;
	before = d->layout;
	focus_before = d->focused;
	tabs_before = d->tab_focus;
	d->tab_focus = 0;
	if (group >= 0) {
		n = &d->layout.nodes[group];
		r = d->boxes[group];
		d->focused = hit;
		floating = fuse_dock_float_root(d, group);
		grip = floating >= 0 ? fminf(24, r.w * 0.25f) : 0;
		if (floating >= 0 && d->layout.nodes[floating].order != FUSE_DOCK_MAX) {
			fuse_dock_raise(d, floating);
			d->revision++;
		}
		if (e->y < r.y + FUSE_DOCK_TITLE) {
			d->tab_focus = 1;
			d->capture = floating >= 0 && e->x >= r.x + r.w - grip ? 2 : 1;
			tab = (int)((e->x - r.x) / fmaxf(1, (r.w - grip) / n->count));
			if (tab >= n->count) tab = n->count - 1;
			if (d->capture == 1 && n->active != tab) {
				n->active = tab;
				d->revision++;
			}
			d->focused = n->tabs[n->active];
		} else if (floating >= 0) {
			r = d->boxes[floating];
			if (e->x >= r.x + r.w - 12 && e->y >= r.y + r.h - 12)
				d->capture = 3;
		}
	}
	if (!d->capture) {
		for (i = 0; i < FUSE_DOCK_NODES; i++) {
			n = &d->layout.nodes[i];
			if (n->kind < 2 || fuse_dock_float_root(d, i) != top) continue;
			a = d->boxes[n->a];
			r = d->boxes[i];
			if (n->kind == 2) { r.x = a.x + a.w; r.w = 4; }
			else { r.y = a.y + a.h; r.h = 4; }
			if (fuse_dock_contains(r, e->x, e->y)) {
				group = i;
				d->capture = 4;
				break;
			}
		}
	}
	if (!d->capture) return top >= 0 && group < 0;
	d->rollback = before;
	d->rollback_focus = focus_before;
	d->rollback_tab_focus = tabs_before;
	d->node = d->capture == 2 || d->capture == 3 ? fuse_dock_float_root(d, group) : group;
	d->window = d->focused;
	d->start_box = d->boxes[d->node];
	d->start_x = e->x;
	d->start_y = e->y;
	d->dragging = 0;
	return 1;
}

int
fuse_dock_busy(const FuseDock *d) { return d->capture != 0; }
int
fuse_dock_revision(const FuseDock *d) { return d->revision; }
int
fuse_dock_focused(const FuseDock *d) { return d->focused; }
int
fuse_dock_focus_tabs(FuseDock *d, int window)
{
	FuseDockNode *n;
	int group, i, floating;
	if (!d || d->capture || window < 0 || window >= d->count) return 0;
	group = fuse_dock_group(d, window);
	if (group < 0) return 0;
	n = &d->layout.nodes[group];
	for (i = 0; n->tabs[i] != window; i++) {}
	if (n->active != i) { n->active = i; d->revision++; }
	floating = fuse_dock_float_root(d, group);
	if (floating >= 0 && d->layout.nodes[floating].order != FUSE_DOCK_MAX) {
		fuse_dock_raise(d, floating);
		d->revision++;
	}
	d->focused = window;
	d->tab_focus = 1;
	return 1;
}

void
fuse_dock_export(const FuseDock *d, FuseDockLayout *l) { *l = d->layout; }

int
fuse_dock_check(const FuseDock *d, const FuseDockLayout *l, int node, int floating, unsigned *seen, unsigned *windows)
{
	const FuseDockNode *n;
	int i, window;
	if (node < 0 || node >= FUSE_DOCK_NODES || (*seen & (1u << node))) return 0;
	*seen |= 1u << node;
	n = &l->nodes[node];
	if (n->kind < 1 || n->kind > 3 || n->floating != floating) return 0;
	if (floating && (!isfinite(n->rect.x) || !isfinite(n->rect.y) || !isfinite(n->rect.w) || !isfinite(n->rect.h) ||
		n->rect.x < 0 || n->rect.x > 1 || n->rect.y < 0 || n->rect.y > 1 || n->rect.w <= 0 || n->rect.w > 1 || n->rect.h <= 0 || n->rect.h > 1 || n->order < 0 || n->order > FUSE_DOCK_MAX)) return 0;
	if (n->kind > 1) {
		if (!isfinite(n->ratio) || n->ratio <= 0 || n->ratio >= 1) return 0;
		return fuse_dock_check(d, l, n->a, 0, seen, windows) && fuse_dock_check(d, l, n->b, 0, seen, windows);
	}
	if (n->count < 1 || n->count > d->count || n->active < 0 || n->active >= n->count) return 0;
	for (i = 0; i < n->count; i++) {
		window = n->tabs[i];
		if (window < 0 || window >= d->count || (*windows & (1u << window))) return 0;
		*windows |= 1u << window;
	}
	return 1;
}

int
fuse_dock_import(FuseDock *d, const FuseDockLayout *l)
{
	unsigned seen = 0, windows = 0, orders = 0;
	int i;
	if (!d || !l || l->root < -1 || l->root >= FUSE_DOCK_NODES) return 0;
	if (l->root >= 0 && !fuse_dock_check(d, l, l->root, 0, &seen, &windows)) return 0;
	for (i = 0; i < FUSE_DOCK_NODES; i++) {
		if (!l->nodes[i].kind || !l->nodes[i].floating) continue;
		if (!fuse_dock_check(d, l, i, 1, &seen, &windows)) return 0;
		if (orders & (1u << l->nodes[i].order)) return 0;
		orders |= 1u << l->nodes[i].order;
	}
	for (i = 0; i < FUSE_DOCK_NODES; i++)
		if (l->nodes[i].kind && !(seen & (1u << i))) return 0;
	if (windows != (1u << d->count) - 1) return 0;
	d->layout = *l;
	d->capture = 0;
	d->focused = fuse_dock_visible(d, 0);
	d->tab_focus = 0;
	d->revision++;
	fuse_dock_layout(d, d->host);
	return 1;
}

int
fuse_dock_begin(FuseDock *d, FuseCanvas canvas, int window, const FuseClass *cls)
{
	FuseDockRect r;
	if (!fuse_dock_content(d, window, &r)) return 0;
	fuse_id(canvas, d->windows[window].name);
	fuse_div_begin(canvas, r.x, r.y, r.w, r.h, cls);
	fuse_div_scope(canvas);
	return 1;
}

void
fuse_dock_end(FuseCanvas canvas) { fuse_div_end(canvas); }

void
fuse_dock_chrome(FuseDock *d, FuseCanvas canvas, int window,
	uint32_t background, uint32_t inactive, uint32_t active, uint32_t text,
	uint32_t grip, uint32_t accent)
{
	FuseDockRect r;
	FuseDockNode *n;
	float width;
	int i, floating;
	if (!fuse_dock_group_rect(d, window, &r) || r.w <= 0 || r.h <= 0) return;
	n = &d->layout.nodes[fuse_dock_group(d, window)];
	floating = fuse_dock_float_root(d, fuse_dock_group(d, window)) >= 0;
	fuse_div_begin(canvas, r.x, r.y, r.w, r.h, NULL);
	fuse_rect(canvas, 0, 0, r.w, r.h, background);
	width = fmaxf(1, (r.w - (floating ? fminf(24, r.w * 0.25f) : 0)) / n->count);
	for (i = 0; i < n->count; i++) {
		fuse_rect(canvas, i * width, 0, width - 1, FUSE_DOCK_TITLE, i == n->active ? active : inactive);
		fuse_div_begin(canvas, i * width, 0, width, FUSE_DOCK_TITLE, NULL);
		if (accent && i == n->active) {
			fuse_rect(canvas, 0, FUSE_DOCK_TITLE - 1, width - 1, 1, accent);
			if (d->tab_focus && d->focused == window) {
				fuse_rect(canvas, 0, 0, width - 1, 2, accent);
				fuse_rect(canvas, 0, 0, 2, FUSE_DOCK_TITLE, accent);
				fuse_rect(canvas, width - 3, 0, 2, FUSE_DOCK_TITLE, accent);
				fuse_rect(canvas, 0, FUSE_DOCK_TITLE - 2, width - 1, 2, accent);
			}
		}
		fuse_text(canvas, 6, 8, 1, d->windows[n->tabs[i]].title, text);
		fuse_div_end(canvas);
	}
	if (floating) {
		fuse_text(canvas, r.w - 19, 8, 1, "::", text);
		fuse_rect(canvas, r.w - 10, r.h - 3, 8, 2, grip);
	}
	fuse_div_end(canvas);
}

void
fuse_dock_feedback(FuseDock *d, FuseCanvas canvas, uint32_t color)
{
	FuseDockRect r;
	int group;
	if (d->capture != 1 || !d->dragging) return;
	group = fuse_dock_group(d, d->target);
	if (group < 0 || d->host.w <= 0 || d->host.h <= 0 ||
		!fuse_dock_can_split(d, d->window, group, d->side)) return;
	if (group == d->node && d->layout.nodes[group].count == 1) return;
	r = d->boxes[group];
	if (d->side == FUSE_DOCK_LEFT || d->side == FUSE_DOCK_RIGHT) {
		r.w *= 0.5f;
		if (d->side == FUSE_DOCK_RIGHT) r.x += r.w;
	} else if (d->side == FUSE_DOCK_TOP || d->side == FUSE_DOCK_BOTTOM) {
		r.h *= 0.5f;
		if (d->side == FUSE_DOCK_BOTTOM) r.y += r.h;
	}
	if (r.w <= 0 || r.h <= 0) return;
	fuse_rect(canvas, r.x, r.y, r.w, 3, color);
	fuse_rect(canvas, r.x, r.y + r.h - 3, r.w, 3, color);
	fuse_rect(canvas, r.x, r.y, 3, r.h, color);
	fuse_rect(canvas, r.x + r.w - 3, r.y, 3, r.h, color);
}

#endif /* FUSE_IMPLEMENTATION_ONCE */
#endif /* FUSE_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
*/
