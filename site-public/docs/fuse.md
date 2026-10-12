# Fuse

Documentation: https://example.org/docs/fuse/
Description: Set up caller-backed canvas storage, feed input, and render Fuse commands.


# Fuse

Fuse is an immediate-mode UI library. Each frame you describe the interface with function calls
(divs, buttons, sliders, text boxes) and Fuse lays it out, handles pointer and keyboard input,
and returns a flat list of draw commands: rectangles, lines, triangles, clip regions, images and
text runs. It does not open windows or draw pixels; your renderer consumes the commands.

All state lives in one caller-provided buffer. Fuse does not allocate. Define
`FUSE_IMPLEMENTATION` in exactly one translation unit before including `Fuse.h`, compile as C99
or later, and link with `-lm`.

## Building a Frame

The first thing to try is a button that reports a click. This program runs headless: it feeds
three frames of pointer input, as a window's event loop would, and prints what happened:

```c
#include <stdio.h>
#include <stdlib.h>
#define FUSE_IMPLEMENTATION
#include "Fuse.h"

static bool frame(FuseCanvas ui, FusePointerState pointer, float x, float y)
{
    static char label[] = "Apply";
    size_t count = 0;

    fuse_canvas_pointer(ui, pointer, x, y);
    fuse_canvas_clear(ui);

    fuse_id(ui, "apply");
    bool clicked = fuse_button_text(ui, label, 20, 20, 120, 36,
                                    0xFF3A6EA5u, 0xFF5A8EC5u);

    FuseCmd *commands = fuse_canvas_draw(ui, &count);
    if (!commands) {
        fprintf(stderr, "fuse error %d\n", (int)fuse_canvas_error(ui));
        return false;
    }
    printf("%zu commands, first is a %.0fx%.0f rectangle\n",
           count, commands[0].rect.w, commands[0].rect.h);
    return clicked;
}

int main(void)
{
    FuseParams params = {.max_elements = 256, .max_windows = 1, .max_scopes = 16};
    size_t bytes = fuse_memory(&params);
    void *memory = malloc(bytes);
    if (!memory) return 1;

    FuseCanvas ui = fuse_place_in_memory(memory, bytes, &params);
    if (!ui) {
        free(memory);
        return 1;
    }
    fuse_canvas_resize(ui, 320, 200);

    frame(ui, FUSE_POINTER_NONE, 60, 40);       /* pointer moves over the button */
    frame(ui, FUSE_POINTER_PRESSED, 60, 40);    /* button pressed */
    if (frame(ui, FUSE_POINTER_RELEASED, 60, 40))
        printf("Apply clicked\n");

    free(memory);
    return 0;
}
```

`FuseParams` sets fixed capacities: `max_elements` counts every element built in a frame,
including the implicit root and each run of a text label; `max_scopes` bounds how deeply divs
nest; `max_windows` bounds docking windows. `fuse_memory()` returns the bytes needed and
`fuse_place_in_memory()` places the canvas in your buffer, which must be 8-byte aligned and stay
alive while the canvas is used. There is nothing to destroy: free the buffer when you are done.

Every frame follows the same order: report input (`fuse_canvas_pointer()`, plus wheel, text and
keys), call `fuse_canvas_clear()`, build the interface, then call `fuse_canvas_draw()`. Widgets
answer input using the boxes laid out in the previous frame, which is why the click arrives on
the frame where the pointer is released over the button. The returned commands live in the
canvas buffer until the next clear.

## Drawing the Commands

Each `FuseCmd` has a `type` and a matching union member. Coordinates are canvas pixels and colors
are `0xAARRGGBB`. Draw the commands in order; `z` is the layer set with `fuse_canvas_layer()`, if
you want to sort by it:

```c
size_t count = 0;
FuseCmd *commands = fuse_canvas_draw(ui, &count);
for (size_t i = 0; i < count; ++i) {
    const FuseCmd *c = &commands[i];
    switch (c->type) {
    case FUSE_CMD_RECT:       fill_rect(c->rect.x, c->rect.y, c->rect.w, c->rect.h, c->rect.color); break;
    case FUSE_CMD_LINE:       draw_line(&c->line); break;
    case FUSE_CMD_TRIANGLE:   fill_triangle(&c->triangle); break;
    case FUSE_CMD_CLIP_START: push_clip(c->clip.x, c->clip.y, c->clip.w, c->clip.h); break;
    case FUSE_CMD_CLIP_END:   pop_clip(); break;
    case FUSE_CMD_IMAGE:      draw_image(c->image.handle, c->image.x, c->image.y, c->image.w, c->image.h); break;
    case FUSE_CMD_TEXT:       draw_text_run(&c->text); break;
    }
}
```

The drawing functions are your renderer's. Clip commands nest; intersect each new clip with the
current one. `FUSE_CMD_IMAGE` carries the handle you passed to `fuse_image()`; Fuse never sees the
pixels. `FUSE_CMD_TEXT` comes from text boxes: `c->text.slot` indexes the `FuseFieldRun` array
returned by `fuse_canvas_fields()`, which holds the string, size, color and caret position for a
proportional font.

`FuseRend.h` is an optional adapter that tessellates these commands for [Rend](/docs/rend/) and
draws text with [Type](/docs/type/). It has its own implementation macro,
`FUSE_REND_IMPLEMENTATION`.

## Laying Out with Classes

A div is a rectangle that contains other elements. Without a class, children are placed at the
coordinates you pass, relative to the div. With a `FuseClass`, the div arranges its children in a
row or column, with padding and gaps, and the coordinates you pass are ignored:

```c
static const FuseClass toolbar = {
    .color = 0xFF202830u,
    .direction = FUSE_DIRECTION_ROW,
    .gap = 8,
    .pad_l = 8, .pad_r = 8, .pad_t = 6, .pad_b = 6,
    .align_y = FUSE_ALIGN_CENTER,
};
static char open_label[] = "Open", save_label[] = "Save", quit_label[] = "Quit";

fuse_div_begin(ui, 0, 0, 640, 48, &toolbar);
    fuse_id(ui, "open");
    bool open = fuse_button_text(ui, open_label, 0, 0, 80, 32, 0xFF405060u, 0xFF607080u);
    fuse_id(ui, "save");
    bool save = fuse_button_text(ui, save_label, 0, 0, 80, 32, 0xFF405060u, 0xFF607080u);
    fuse_space(ui);   /* takes the leftover width */
    fuse_id(ui, "quit");
    bool quit = fuse_button_text(ui, quit_label, 0, 0, 80, 32, 0xFF804040u, 0xFFA06060u);
fuse_div_end(ui);
```

A class's `width_sizing` and `height_sizing` decide how children are sized on each axis:
`FUSE_SIZING_FIT` keeps the size you passed, `FUSE_SIZING_FIXED` uses the class's `width` or
`height` in pixels, `FUSE_SIZING_PERCENT` uses them as a fraction of the div's inner size, and
`FUSE_SIZING_GROW` shares the leftover space. `fuse_sizing()` overrides the mode for the next
element only. `min_*` and `max_*` clamp the results; `color` paints the div's background (zero
means none).

For side-by-side columns, `fuse_col_begin()` splits a width by ratios. Build the first column,
call `fuse_col_switch()` for each next column, and finish with `fuse_col_end()`; the row takes the
height of its tallest column:

```c
static const float split[] = {0.3f, 0.7f};
static const FuseClass stack = {.direction = FUSE_DIRECTION_COLUMN, .gap = 4};

fuse_col_begin(ui, 0, 48, 640, &stack, split, 2);
    fuse_text(ui, 0, 0, 2, "Layers", 0xFFFFFFFFu);
    fuse_col_switch(ui);
    fuse_text(ui, 0, 0, 2, "Preview", 0xFFFFFFFFu);
fuse_col_end(ui);
```

`fuse_text()` draws the built-in 5x7 bitmap font, upper case only, scaled by an integer factor.
Each lit run of pixels becomes a rectangle command, so it needs no font or texture.

## Identifying Widgets

Every element gets an ID. Without `fuse_id()`, the ID comes from the parent and the element's
position among its siblings, so it changes when the tree above it changes shape. Call
`fuse_id(ui, "name")` (or `fuse_idi(ui, "row", i)` in loops) before any widget whose interaction
must survive such changes. Using the same ID twice in one frame fails the frame with
`FUSE_ERR_DUPLICATE_ID`.

Names are global to the canvas by default. After opening a named div, `fuse_div_scope()` makes the
names of its children local to it, so two panels can each contain a button called `"ok"`.
`fuse_element_is_hovered()` asks about a named element in the current scope.

## Sliders, Scrolling and Text Input

Widgets keep no values themselves; they read and write your variables:

```c
static float volume = 0.5f, scroll = 0.0f;
static char name[64] = "";
static int caret = -1;
static const FuseClass list = {.direction = FUSE_DIRECTION_COLUMN, .gap = 2};

fuse_id(ui, "volume");
fuse_slider(ui, 20, 80, 200, 16, 0xFF303A44u, 0xFFE0E0E0u, &volume);

fuse_id(ui, "list");
fuse_div_begin_scroll(ui, 20, 110, 200, 120, &list, &scroll);
for (int i = 0; i < 40; ++i)
    fuse_rect(ui, 0, 0, 200, 18, i % 2 ? 0xFF2A2A2Au : 0xFF333333u);
fuse_div_end(ui);

fuse_id(ui, "name");
if (fuse_textbox(ui, 240, 80, 200, 28, 6, 6, name, sizeof name, &caret,
                 "Your name", 16, 0xFF1E1E1Eu, 0xFF262626u, 0xFFFFFFFFu, 0xFF808080u,
                 1, NULL, NULL)) {
    /* name changed this frame */
}
```

The slider keeps its knob position in `0..1` and follows the pointer while it is held. A
scrolling div clips its children and stores its offset in `scroll`; feed wheel motion with
`fuse_canvas_wheel()` before building. A text box is focused by clicking it (or with
`fuse_focus()`); pass typed UTF-8 with `fuse_canvas_text()` and editing keys such as
`FUSE_KEY_BACKSPACE` with `fuse_canvas_key()`. The box edits `name` in place, keeps the
terminating NUL within `sizeof name`, and returns `true` when the text changed. By default it
accepts bytes 32 to 126; pass an `allow` callback to change that.

Other building blocks: `fuse_rect()` and `fuse_image()` place plain boxes,
`fuse_radial()` draws a ring menu and returns the hovered slice, and
`fuse_canvas_input_enabled()` turns off pointer handling for the widgets that follow, for
example behind a modal panel.

## Docking Windows

The docking API arranges named panels in tabs and splits that the user can drag, resize and
float. The dock lives in its own caller buffer, which must be 8-byte aligned:

```c
static uint64_t dock_memory[4096];
static const FuseClass panel = {.color = 0xFF1A1D22u, .pad_l = 8, .pad_t = 8};

if (fuse_dock_memory() > sizeof dock_memory) return 1;
FuseDock *dock = fuse_dock_create(dock_memory, sizeof dock_memory, 2);
if (!dock) return 1;
int scene = fuse_dock_register(dock, "scene", "Scene", 200, 120);
int props = fuse_dock_register(dock, "props", "Properties", 160, 120);
if (scene < 0 || props < 0 || !fuse_dock_place(dock, props, scene, FUSE_DOCK_RIGHT)) return 1;

/* Every frame, after fuse_canvas_clear(): */
FuseDockEvent event = {.x = mouse_x, .y = mouse_y, .press = pressed, .release = released};
fuse_dock_layout(dock, (FuseDockRect){0, 0, 1280, 720});
fuse_dock_event(dock, &event);
int window;
for (int i = 0; (window = fuse_dock_visible(dock, i)) >= 0; ++i) {
    fuse_dock_chrome(dock, ui, window, 0xFF1A1D22u, 0xFF2A2E35u, 0xFF3A404Au,
                     0xFFE0E0E0u, 0xFF808080u, 0xFF4A90E2u);
    if (fuse_dock_begin(dock, ui, window, &panel)) {
        /* build the panel's widgets here */
        fuse_dock_end(ui);
    }
}
fuse_dock_feedback(dock, ui, 0x804A90E2u);
```

`mouse_x`, `pressed` and the other event fields come from your platform layer. `fuse_dock_event()`
returns nonzero when the dock consumed the input, so you can keep it from the panels.
`fuse_dock_export()` and `fuse_dock_import()` save and restore the arrangement as a plain
`FuseDockLayout` value.

## Handling Errors

If a frame overflows a capacity, reuses an ID, or leaves a div or column open,
`fuse_canvas_draw()` returns NULL with a count of zero and cancels that frame's input.
`fuse_canvas_error()` reports the reason (`FUSE_ERR_OVERFLOW`, `FUSE_ERR_DUPLICATE_ID`,
`FUSE_ERR_UNBALANCED`), and the next `fuse_canvas_clear()` resets it. `fuse_canvas_debug(ui, true)`
overlays element boxes and a tree view of the frame.

See [Fuse symbols](/symbols/fuse/) for the generated declarations and header comments.
