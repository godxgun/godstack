+++
title = 'Fuse symbols'
description = 'Types, macros, and functions declared by Fuse.h.'
weight = 70
[params]
  id = 'symbols/fuse'
  product = 'Fuse'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Fuse symbols

For setup and API workflows, read the [Fuse guide](/docs/fuse/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Fuse.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### FuseDirection {#fusedirection}

Source: `Fuse.h:92`

```c
typedef enum {
    FUSE_DIRECTION_ROW = 0,
    FUSE_DIRECTION_COLUMN,
} FuseDirection;
```

#### FusePointerState {#fusepointerstate}

Source: `Fuse.h:97`

```c
typedef enum {
    FUSE_POINTER_NONE = 0,
    FUSE_POINTER_RELEASED,
    FUSE_POINTER_PRESSED,
    FUSE_POINTER_ALT,
} FusePointerState;
```

#### FuseSizing {#fusesizing}

Source: `Fuse.h:104`

```c
typedef enum {
    FUSE_SIZING_FIT = 0,      /* shrink-wrap children */
    FUSE_SIZING_GROW,         /* take leftover space on that axis */
    FUSE_SIZING_FIXED,        /* width/height are pixels */
    FUSE_SIZING_PERCENT,      /* width/height are 0..1 of current screen minus padding/gaps */
} FuseSizing;
```

#### FuseAlign {#fusealign}

Source: `Fuse.h:111`

```c
typedef enum {
    FUSE_ALIGN_START = 0,     /* left / top */
    FUSE_ALIGN_CENTER,
    FUSE_ALIGN_END,           /* right / bottom */
} FuseAlign;
```

#### FuseError {#fuseerror}

Source: `Fuse.h:117`

```c
typedef enum {
    FUSE_ERR_OK = 0,
    FUSE_ERR_BUF_TOO_SMALL,   /* insufficient or unaligned backing */
    FUSE_ERR_OVERFLOW,        /* a widget/cmd would exceed max_elements or cmd capacity */
    FUSE_ERR_DUPLICATE_ID,    /* same id emitted twice in one generation */
    FUSE_ERR_UNBALANCED,      /* div_end with empty stack, or draw with open divs */
} FuseError;
```

#### FuseCmdType {#fusecmdtype}

Source: `Fuse.h:125`

```c
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
```

#### FuseCmdRect {#fusecmdrect}

Source: `Fuse.h:136`

```c
typedef struct {
    float x, y, w, h;
    uint32_t color;
} FuseCmdRect;
```

#### FuseCmdLine {#fusecmdline}

Source: `Fuse.h:141`

```c
typedef struct {
    float x1, y1, x2, y2;
    float thickness;
    uint32_t color;
} FuseCmdLine;
```

#### FuseCmdClip {#fusecmdclip}

Source: `Fuse.h:147`

```c
typedef struct {
    float x, y, w, h;
} FuseCmdClip;
```

#### FuseCmdTriangle {#fusecmdtriangle}

Source: `Fuse.h:151`

```c
typedef struct {
    float x1, y1, x2, y2, x3, y3;
    uint32_t color;
} FuseCmdTriangle;
```

#### FuseCmdImage {#fusecmdimage}

Source: `Fuse.h:161`

```c
typedef struct {
    float x, y, w, h;
    uint32_t handle;
} FuseCmdImage;
```

Laid-out image box, like an img with a width and height. x,y,w,h are
canvas pixels and are not snapped. Handle is the caller's.
Fuse does not store pixels.

#### FuseCmdText {#fusecmdtext}

Source: `Fuse.h:169`

```c
typedef struct {
    float x, y, w, h;
    uint32_t slot;
} FuseCmdText;
```

Proportional text. The command is only the clip box plus a slot.
Glyphs, caret, and the string live in FuseFieldRun so this union
does not enlarge the command union.

#### FuseFieldRun {#fusefieldrun}

Source: `Fuse.h:176`

```c
typedef struct FuseFieldRun {
    const char *str;
    float size;
    int32_t caret;
    uint32_t color;
    uint32_t caret_color;
} FuseFieldRun;
```

One text box for this frame. str stays valid through draw.
caret is a byte index, or -1 when hidden.

#### FuseCmd {#fusecmd}

Source: `Fuse.h:190`

```c
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
```

FuseCmd is the basic unit emited by fuse that your renderer will turn
into One draw record. draw() returns these packed, already in canvas space,
in emit order; z is the explicit caller layer (default 0). id is the widget
that produced the command, or 0 for anonymous geometry.

#### FuseClass {#fuseclass}

Source: `Fuse.h:212`

```c
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
```

█
█
███ █  ███ ███ ███
█   █    █ █   █
█   █  ███   █   █
███ ██ ███ ███ ███

Reusable style passed down to elements and their children (in the case of divs).

#### FuseCanvas {#fusecanvas}

Source: `Fuse.h:246`

```c
typedef struct fuse_canvas_t *FuseCanvas;
```

███ ███ ███ █ █ ███ ███
█     █ █ █ █ █   █ █
█   ███ █ █ █ █ ███   █
███ ███ █ █  █  ███ ███

The canvas is the "context" of Fuse UI and it represents
where elements are "drawn" to.

+------------------------------------------------+
| [Button]                                       |
| Text                                           |
| 0.5 |---o---|                                  |
|                                                |
|                                                |
+------------------------------------------------+
Opaque

#### FuseParams {#fuseparams}

Source: `Fuse.h:255`

```c
typedef struct FuseParams {
    size_t max_elements;
    size_t max_windows;
    size_t max_scopes;
} FuseParams;
```

#### FuseKey {#fusekey}

Source: `Fuse.h:283`

```c
typedef enum {
    FUSE_KEY_BACKSPACE = 1,
    FUSE_KEY_DELETE,
    FUSE_KEY_LEFT,
    FUSE_KEY_RIGHT,
    FUSE_KEY_HOME,
    FUSE_KEY_END,
} FuseKey;
```

Editing keys for the focused text box. Queued until that box is built.

#### FuseDock {#fusedock}

Source: `Fuse.h:473`

```c
typedef struct FuseDock FuseDock;
```

#### FuseDockRect {#fusedockrect}

Source: `Fuse.h:474`

```c
typedef struct FuseDockRect { float x, y, w, h; } FuseDockRect;
```

#### FuseDockSide {#fusedockside}

Source: `Fuse.h:475`

```c
typedef enum {
	FUSE_DOCK_CENTER, FUSE_DOCK_LEFT, FUSE_DOCK_RIGHT,
	FUSE_DOCK_TOP, FUSE_DOCK_BOTTOM, FUSE_DOCK_FLOAT
} FuseDockSide;
```

#### FuseDockNode {#fusedocknode}

Source: `Fuse.h:482`

```c
typedef struct FuseDockNode {
	int kind, a, b, count, active, floating, order;
	int tabs[FUSE_DOCK_MAX];
	float ratio;
	FuseDockRect rect;
} FuseDockNode;
```

Node kind: 0 unused, 1 group, 2 horizontal children, 3 vertical children.
Group tabs are registration indices; file formats must map stable names.
Floating group rectangles are fractions of the host, not pixels.

#### FuseDockLayout {#fusedocklayout}

Source: `Fuse.h:488`

```c
typedef struct FuseDockLayout {
	int root;
	FuseDockNode nodes[FUSE_DOCK_NODES];
} FuseDockLayout;
```

#### FuseDockEvent {#fusedockevent}

Source: `Fuse.h:497`

```c
typedef struct FuseDockEvent {
	float x, y;
	int press, release, cancel, previous, next, activate;
} FuseDockEvent;
```

Pointer coordinates are absolute host coordinates. cancel covers Escape,
focus loss and modal activation. previous/next automatically activate tabs
only after a tab/title click or fuse_dock_focus_tabs; content clicks leave
keys to the panel.
activate consumes Enter/Space without changing an already selected tab.

### Functions

#### fuse_memory {#fuse_memory}

Source: `Fuse.h:261`

```c
size_t     fuse_memory(const FuseParams *params);
```

#### fuse_place_in_memory {#fuse_place_in_memory}

Source: `Fuse.h:262`

```c
FuseCanvas fuse_place_in_memory(void *buf, size_t bytes, const FuseParams *params);
```

#### fuse_canvas_max_windows {#fuse_canvas_max_windows}

Source: `Fuse.h:263`

```c
size_t     fuse_canvas_max_windows(FuseCanvas);
```

#### fuse_canvas_scope_peak {#fuse_canvas_scope_peak}

Source: `Fuse.h:265`

```c
size_t     fuse_canvas_scope_peak(FuseCanvas);
```

Highest total stack depth in this build, reset to the root (1) on clear.

#### fuse_canvas_error {#fuse_canvas_error}

Source: `Fuse.h:266`

```c
FuseError  fuse_canvas_error(FuseCanvas);
```

#### fuse_canvas_memory {#fuse_canvas_memory}

Source: `Fuse.h:269`

```c
size_t     fuse_canvas_memory(size_t max_elements);
```

Legacy helpers: 16 window registrations and min(max_elements,32) scopes.
create infers the greatest fitting element capacity; no allocation/fallback.

#### fuse_canvas_create {#fuse_canvas_create}

Source: `Fuse.h:270`

```c
FuseCanvas fuse_canvas_create(void *buf, size_t bufsize);
```

#### fuse_canvas_clear {#fuse_canvas_clear}

Source: `Fuse.h:271`

```c
void       fuse_canvas_clear(FuseCanvas);
```

#### fuse_canvas_layer {#fuse_canvas_layer}

Source: `Fuse.h:273`

```c
void       fuse_canvas_layer(FuseCanvas, int16_t layer);
```

Applies to subsequently built elements. Resets to 0 on clear.

#### fuse_canvas_input_enabled {#fuse_canvas_input_enabled}

Source: `Fuse.h:277`

```c
void       fuse_canvas_input_enabled(FuseCanvas, int enabled);
```

Gate pointer/wheel picking and captured slider updates for subsequent widgets.
Keyboard focus/edits survive; capture still ends on release. Resets on clear.
This is a builder setting, not a stack: callers restore it after a panel.

#### fuse_canvas_resize {#fuse_canvas_resize}

Source: `Fuse.h:278`

```c
void       fuse_canvas_resize(FuseCanvas, float w, float h);
```

#### fuse_canvas_pointer {#fuse_canvas_pointer}

Source: `Fuse.h:279`

```c
void       fuse_canvas_pointer(FuseCanvas, FusePointerState pointer_state, float x, float y);
```

#### fuse_canvas_wheel {#fuse_canvas_wheel}

Source: `Fuse.h:280`

```c
void       fuse_canvas_wheel(FuseCanvas, float dx, float dy);
```

+dy is wheel up

#### fuse_canvas_text {#fuse_canvas_text}

Source: `Fuse.h:292`

```c
void       fuse_canvas_text(FuseCanvas, const char *utf8);
```

#### fuse_canvas_key {#fuse_canvas_key}

Source: `Fuse.h:293`

```c
void       fuse_canvas_key(FuseCanvas, FuseKey key);
```

#### fuse_focus {#fuse_focus}

Source: `Fuse.h:296`

```c
void       fuse_focus(FuseCanvas, const char *name);
```

NULL clears. The name is the fuse_id of a text box in the current ID scope.
Focus queries and textbox_edit must run inside that same scope.

#### fuse_focused {#fuse_focused}

Source: `Fuse.h:297`

```c
bool       fuse_focused(FuseCanvas, const char *name);
```

#### fuse_canvas_draw {#fuse_canvas_draw}

Source: `Fuse.h:302`

```c
FuseCmd   *fuse_canvas_draw(FuseCanvas, size_t *cmd_count);
```

Exhaustion/unbalanced/duplicate construction fails the whole build: NULL,
zero commands, canceled capture/focus/queued input, no synthesized release.
Error is sticky through draw, cleared by the next canvas_clear.

#### fuse_canvas_fields {#fuse_canvas_fields}

Source: `Fuse.h:304`

```c
const FuseFieldRun *fuse_canvas_fields(FuseCanvas, size_t *count);
```

Valid until the next clear. Parallel to FUSE_CMD_TEXT slots.

#### fuse_canvas_debug {#fuse_canvas_debug}

Source: `Fuse.h:306`

```c
void       fuse_canvas_debug(FuseCanvas, bool enabled);
```

#### fuse_percent_x {#fuse_percent_x}

Source: `Fuse.h:308`

```c
float fuse_percent_x(FuseCanvas, float p);
```

#### fuse_percent_y {#fuse_percent_y}

Source: `Fuse.h:309`

```c
float fuse_percent_y(FuseCanvas, float p);
```

#### fuse_div_begin {#fuse_div_begin}

Source: `Fuse.h:336`

```c
void fuse_div_begin(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls);
```

█                      █
█                   █
█  ███ █ █ ███ █ █ ███ █ ███ ███
█    █ █ █ █ █ █ █  █  █ █ █ █ █
█  ███ █ █ █ █ █ █  █  █ █ █ █ █
██ ███ ███ ███ ███  ██ █ █ █ ███
█                     █
███                   ███
Divs are fixed rectangles that act like mini canvases. 
You could technachly do anything by using just divs 
and passing the appropiate parameters.
+------------------------------------------------+ 
|    col1 |                                 col2 |
|         |                 _______              |
|_________|                |   div |             |
|     div |                |_______|             |
|         |                                      |
|         |                                      |
+------------------------------------------------+ 
Columns allow you to split up space horizontally according
to specific ratios like [0.3, 0.7] for example.

#### fuse_div_begin_scroll {#fuse_div_begin_scroll}

Source: `Fuse.h:337`

```c
void fuse_div_begin_scroll(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls, float *scroll);
```

#### fuse_div_scope {#fuse_div_scope}

Source: `Fuse.h:343`

```c
void fuse_div_scope(FuseCanvas);
```

After opening a stable named div, scope explicitly named child IDs to its ID.
Call before child IDs/queries. Ordinary nested divs inherit; a nested scope
combines its own scoped ID with child names. div_end restores the outer scope.
No scope means names remain canvas-global (legacy behavior). Anonymous child
IDs already derive from parent IDs. Requires an open div; no extra storage.

#### fuse_scope_enter {#fuse_scope_enter}

Source: `Fuse.h:349`

```c
uint32_t fuse_scope_enter(FuseCanvas, const char *name);
```

Temporarily enter a named child scope without emitting a layout element.
Resolves name against the current scope, exactly like id + div_scope.
Pair with restore(previous) in LIFO order before closing enclosing divs or
building other panels. Tokens are canvas-local; clear resets this override.
Intended for focus/edit queries outside panel building; no allocation.

#### fuse_scope_restore {#fuse_scope_restore}

Source: `Fuse.h:350`

```c
void fuse_scope_restore(FuseCanvas, uint32_t previous);
```

#### fuse_div_end {#fuse_div_end}

Source: `Fuse.h:351`

```c
void fuse_div_end(FuseCanvas);
```

#### fuse_col_begin {#fuse_col_begin}

Source: `Fuse.h:352`

```c
void fuse_col_begin(FuseCanvas, float x, float y, float w, const FuseClass *cls, const float *ratios, uint32_t n);
```

#### fuse_col_switch {#fuse_col_switch}

Source: `Fuse.h:353`

```c
void fuse_col_switch(FuseCanvas);
```

#### fuse_col_end {#fuse_col_end}

Source: `Fuse.h:354`

```c
void fuse_col_end(FuseCanvas);
```

#### fuse_window_begin {#fuse_window_begin}

Source: `Fuse.h:379`

```c
void fuse_window_begin(FuseCanvas, char *name, float x, float y, float w, float h, const FuseClass *cls);
```

Windows / docking is a necessary feature of modern editing software.
Maybe in a 2.0 version of Fuse, I could find a way to merge divs
and windows, but I don't find that necessary or an issue.

CANVAS
+-----------------------+------------------------+ 
|  _______           DIV|            |    DOCKING|
| |   _[]X|     _______ |            |           |
| |       |    |   _[]X||           <|>          |
| |       |    |       ||           <|>          |
| |_______|    |       ||            |           |    
|              |_______||win1        |win2       |                
+------------------------------------------------+ 

Windows act similar to divs in the sense that they also act as 
containers for the elements drawn inside of them.

A docking div would allow you to customize how you want windows to
be docked / tiled. Otherwise, windows will act in a "floating" manner
similar to what is seen in non-tiling desktop environments like Windows.
Standalone named div wrappers; persistent docking API is below.

#### fuse_window_begin_scroll {#fuse_window_begin_scroll}

Source: `Fuse.h:380`

```c
void fuse_window_begin_scroll(FuseCanvas, float x, float y, float w, float h, const FuseClass *cls, float *scroll);
```

#### fuse_window_end {#fuse_window_end}

Source: `Fuse.h:381`

```c
void fuse_window_end(FuseCanvas);
```

#### fuse_id {#fuse_id}

Source: `Fuse.h:392`

```c
void fuse_id(FuseCanvas, const char *name);
```

█
█                     █
███ █  ███ █████ ███ ███ ███ ███
███ █  ███ █ █ █ ███ █ █  █  █
█   █  █   █ █ █ █   █ █  █    █
███ ██ ███ █ █ █ ███ █ █  ██ ███

id functions stem the next element with a name in the current div ID scope.
Hover/focus/edit queries resolve names in that same scope.

#### fuse_idi {#fuse_idi}

Source: `Fuse.h:393`

```c
void fuse_idi(FuseCanvas, const char *name, int index);
```

#### fuse_element_is_hovered {#fuse_element_is_hovered}

Source: `Fuse.h:394`

```c
bool fuse_element_is_hovered(FuseCanvas, const char *name);
```

#### fuse_sizing {#fuse_sizing}

Source: `Fuse.h:399`

```c
void fuse_sizing(FuseCanvas, uint8_t width_sizing, uint8_t height_sizing);
```

Stems the next element. Replaces the parent class on that axis.
FIT keeps the size passed in. GROW takes leftover. FIXED and PERCENT
use the size passed in (pixels, or 0..1). Omit the call to inherit.

#### fuse_space {#fuse_space}

Source: `Fuse.h:401`

```c
void fuse_space(FuseCanvas);
```

Empty child. Grows on the parent main axis. Needs a classed parent.

#### fuse_button {#fuse_button}

Source: `Fuse.h:405`

```c
bool fuse_button(FuseCanvas, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);
```

Clicked this frame: last-frame hover + released-this-frame.
selected is the idle fill, hovered the hover/press fill. Local space.

#### fuse_button_text {#fuse_button_text}

Source: `Fuse.h:409`

```c
bool fuse_button_text(FuseCanvas, char *text, float x, float y, float w, float h, uint32_t selected, uint32_t hovered);
```

Button plus centered 5x7 label. The label is a child, recentered from
the final box. Ink is 0xFF000000.

#### fuse_slider {#fuse_slider}

Source: `Fuse.h:410`

```c
void fuse_slider(FuseCanvas, float x, float y, float w, float h, uint32_t track, uint32_t nob, float *nob_pos);
```

#### fuse_textbox {#fuse_textbox}

Source: `Fuse.h:416`

```c
bool fuse_textbox(FuseCanvas, float x, float y, float w, float h, float pad_l, float pad_r,
    char *buf, int cap, int *caret, const char *placeholder, float size,
    uint32_t fill, uint32_t hovered, uint32_t ink, uint32_t ghost, int caret_on,
    int (*allow)(unsigned char ch, void *user), void *user);
```

One-line field. Call fuse_id first. buf is NUL-terminated, cap includes the NUL.
pad_l and pad_r inset the glyphs. placeholder is drawn when buf is empty.
caret_on shows the caret while the box is focused. allow may be NULL,
which keeps bytes 32..126. Returns 1 when buf changes.

#### fuse_textbox_edit {#fuse_textbox_edit}

Source: `Fuse.h:423`

```c
bool fuse_textbox_edit(FuseCanvas, const char *name, char *buf, int cap, int *caret,
    int (*allow)(unsigned char ch, void *user), void *user);
```

Applies the queued edit to buf when name is the focused field.
Use this before layout that depends on the text. The later text box
call draws that result and does not apply the same keys again.

#### fuse_rect {#fuse_rect}

Source: `Fuse.h:433`

```c
void    fuse_rect(FuseCanvas, float x, float y, float w, float h, uint32_t color);
```

█
█
███ ███ ███ █ █ █
█ █ █     █ █ █ █
█ █ █   ███ █ █ █
███ █   ███  █ █

#### fuse_radial_pick {#fuse_radial_pick}

Source: `Fuse.h:443`

```c
int fuse_radial_pick(float cx, float cy, float inner, float outer, int count, float px, float py);
```

Annular menu, 1..8 slices. Slice 0 is centered to the right; indices
increase clockwise in screen coordinates. Inner/outer boundaries cancel;
angular boundaries belong to the following slice. Invalid/nonfinite input
returns -1. Pointer coordinates are relative to the same parent as center.
The caller owns capture, release, labels and actions. No input is consumed.
Drawing uses at most 144 triangle commands; no heap allocation. Link libm.
Rebuild consumers and size canvas buffers with fuse_canvas_memory:
FuseCmd grew by one float. The menu is a fixed-size drawing leaf.

#### fuse_radial {#fuse_radial}

Source: `Fuse.h:444`

```c
int fuse_radial(FuseCanvas, float cx, float cy, float inner, float outer, int count,
    float px, float py, uint32_t idle, uint32_t hovered);
```

#### fuse_image {#fuse_image}

Source: `Fuse.h:449`

```c
void    fuse_image(FuseCanvas, float x, float y, float w, float h, uint32_t handle);
```

Image in the layout, same coordinates as fuse_rect. w and h are the
displayed size (intrinsic pixels times the caller's scale).

#### fuse_text {#fuse_text}

Source: `Fuse.h:461`

```c
void    fuse_text(FuseCanvas, float x, float y, float s, const char *str, uint32_t color);
```

█           █
███ ███ █ █ ███
█  ███  █   █
█  █    █   █
██ ███ █ █  ██

Text, font and bitmaps.
Built-in 5x7 bitmap, the fallback face. One layout element per string;
glyph runs are its children, each glyph pixel s by s. s snaps to an integer.

#### fuse_text_utf8 {#fuse_text_utf8}

Source: `Fuse.h:462`

```c
void    fuse_text_utf8(FuseCanvas, float x, float y, float s, const uint32_t *str, uint32_t color);
```

#### fuse_text_width {#fuse_text_width}

Source: `Fuse.h:463`

```c
float   fuse_text_width(const char *str, float s);
```

#### fuse_dock_memory {#fuse_dock_memory}

Source: `Fuse.h:502`

```c
size_t fuse_dock_memory(void);
```

#### fuse_dock_create {#fuse_dock_create}

Source: `Fuse.h:503`

```c
FuseDock *fuse_dock_create(void *memory, size_t size, int capacity);
```

#### fuse_dock_register {#fuse_dock_register}

Source: `Fuse.h:505`

```c
int fuse_dock_register(FuseDock *, const char *name, const char *title, float min_w, float min_h);
```

Returns registration index, or -1. Names/titles are copied, 31/63 bytes max.

#### fuse_dock_name {#fuse_dock_name}

Source: `Fuse.h:506`

```c
const char *fuse_dock_name(const FuseDock *, int window);
```

#### fuse_dock_title {#fuse_dock_title}

Source: `Fuse.h:507`

```c
const char *fuse_dock_title(const FuseDock *, int window);
```

#### fuse_dock_reset {#fuse_dock_reset}

Source: `Fuse.h:508`

```c
void fuse_dock_reset(FuseDock *);
```

#### fuse_dock_place {#fuse_dock_place}

Source: `Fuse.h:512`

```c
int fuse_dock_place(FuseDock *, int window, int target, FuseDockSide side);
```

Returns 0 without mutation for invalid targets/minima or while captured.
A zero-by-zero host permits constructing defaults before host sizing.
Cancel any pending gesture before programmatically changing placement.

#### fuse_dock_layout {#fuse_dock_layout}

Source: `Fuse.h:514`

```c
void fuse_dock_layout(FuseDock *, FuseDockRect host);
```

Host changes cancel a captured gesture and retain its prepress arrangement.

#### fuse_dock_event {#fuse_dock_event}

Source: `Fuse.h:516`

```c
int fuse_dock_event(FuseDock *, const FuseDockEvent *);
```

Returns 1 for consumed input, including the terminating release/cancel.

#### fuse_dock_busy {#fuse_dock_busy}

Source: `Fuse.h:517`

```c
int fuse_dock_busy(const FuseDock *);
```

#### fuse_dock_revision {#fuse_dock_revision}

Source: `Fuse.h:518`

```c
int fuse_dock_revision(const FuseDock *);
```

#### fuse_dock_focused {#fuse_dock_focused}

Source: `Fuse.h:519`

```c
int fuse_dock_focused(const FuseDock *);
```

#### fuse_dock_focus_tabs {#fuse_dock_focus_tabs}

Source: `Fuse.h:522`

```c
int fuse_dock_focus_tabs(FuseDock *, int window);
```

Give tabs keyboard focus (e.g. host Tab navigation), activating window.
Returns 0 for invalid windows or during capture. Does not reorder tabs.

#### fuse_dock_hit {#fuse_dock_hit}

Source: `Fuse.h:523`

```c
int fuse_dock_hit(const FuseDock *, float x, float y);
```

#### fuse_dock_visible {#fuse_dock_visible}

Source: `Fuse.h:525`

```c
int fuse_dock_visible(const FuseDock *, int position);
```

Visible windows in paint order; -1 after the last.

#### fuse_dock_content {#fuse_dock_content}

Source: `Fuse.h:526`

```c
int fuse_dock_content(const FuseDock *, int window, FuseDockRect *);
```

#### fuse_dock_group_rect {#fuse_dock_group_rect}

Source: `Fuse.h:527`

```c
int fuse_dock_group_rect(const FuseDock *, int window, FuseDockRect *);
```

#### fuse_dock_export {#fuse_dock_export}

Source: `Fuse.h:528`

```c
void fuse_dock_export(const FuseDock *, FuseDockLayout *);
```

#### fuse_dock_import {#fuse_dock_import}

Source: `Fuse.h:531`

```c
int fuse_dock_import(FuseDock *, const FuseDockLayout *);
```

Every registered window must occur exactly once; float orders must be unique.
Rejects without mutation, including focus/capture. Success ends capture.

#### fuse_dock_begin {#fuse_dock_begin}

Source: `Fuse.h:536`

```c
int fuse_dock_begin(FuseDock *, FuseCanvas, int window, const FuseClass *);
```

Builder takes absolute host coordinates and clips content. Its named root
scopes explicitly named children and keeps anonymous child IDs stable across
placement and emission order. Outside building, enter the registered window
name with fuse_scope_enter in the same enclosing scope for focus/edit queries.

#### fuse_dock_end {#fuse_dock_end}

Source: `Fuse.h:537`

```c
void fuse_dock_end(FuseCanvas);
```

#### fuse_dock_chrome {#fuse_dock_chrome}

Source: `Fuse.h:541`

```c
void fuse_dock_chrome(FuseDock *, FuseCanvas, int window,
	uint32_t background, uint32_t inactive, uint32_t active, uint32_t text,
	uint32_t grip, uint32_t accent);
```

Presentation colors are per-call widget arguments, never retained.
accent draws a 1px active-tab underline and a 2px keyboard-focus outline;
pass zero to omit those decorations. Layout/input geometry is unchanged.

#### fuse_dock_feedback {#fuse_dock_feedback}

Source: `Fuse.h:544`

```c
void fuse_dock_feedback(FuseDock *, FuseCanvas, uint32_t color);
```

<!-- END GENERATED API -->
