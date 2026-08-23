#ifndef _FUSE_C_
#define _FUSE_C_
 /*  __________________________________________________________________________
 *  | CmdType | CmdRect     | CmdType | CmdRectBorder   | ...
 *  --------------------------------------------------------------------------
 */


#include "fuse.h"
#include "fuse_internal.h"
#include "fuse_backend.h"

#include "fuse_tween.c"

#include "allocators.c"
#include <stdint.h>
#include <string.h>

#define FUSE_MIN(a, b) ((a < b) ? a : b)
#define FUSE_MAX(a, b) ((a > b) ? a : b)
#define FUSE_LIMIT(n, a, b) (FUSE_MAX(a, FUSE_MIN(b, n)))
#define FUSE_IS_HOVERED(x, y, w, h) (\
            f__canvas->pointer_x >= x && f__canvas->pointer_x <= x + w &&\
            f__canvas->pointer_y >= y && f__canvas->pointer_y <= y + h)

static inline size_t    fuse__element(); // "base class" of all UI widgets
static inline void      fuse__cmdbuffer_push(FuseCmd cmd);
static void             fuse__cmdbuffer_print();

static const char*  fuse__cmd_name(uint8_t type);

extern FuseCanvas
fuse_canvas_create(size_t memory_limit, FuseStyle base_style)
{
    (void) base_style;

    FuseCanvas new = malloc(sizeof *new);
    memset(new, 0, sizeof *new);

    new->memory = malloc(memory_limit);

    al_arena_create(&new->arena, new->memory, memory_limit);

    size_t cmdbuffer_mem = (memory_limit * 8) / 10;
    cmdbuffer_mem -= (cmdbuffer_mem % sizeof *new->cmdbuffer_ptr);  // remove remainder

    FuseCmd *cmdbuffer_ptr = al_arena_alloc(&new->arena, cmdbuffer_mem);
    new->cmdbuffer_ptr = cmdbuffer_ptr; 
    new->cmdbuffer_size = cmdbuffer_mem / sizeof (*cmdbuffer_ptr);
    new->cmdbuffer_head = 0;

    size_t element_data_mem = memory_limit - cmdbuffer_mem;
    void *element_data_ptr   = al_arena_alloc(&new->arena, element_data_mem);

    al_arena_create(&new->element_data_arena, element_data_ptr, element_data_mem);

    fuse_canvas_set(new);
    return new;
}

extern void
fuse_canvas_set(FuseCanvas canvas)
{
    f__canvas = canvas;
}

extern void
fuse_canvas_debug(FuseCanvas canvas, bool debug)
{
    canvas->debug = debug;
}

extern void
fuse_canvas_clear(FuseCanvas canvas)
{
    canvas->cmdbuffer_head = 0;
    canvas->current_id = 0;
    canvas->current_frame++;
}

extern void
fuse_canvas_draw(FuseCanvas canvas, FuseRendererCallback callback, void* data)
{
    for (size_t u = 0; u < f__canvas->cmdbuffer_head; u++) {
        FuseCmd cmd = f__canvas->cmdbuffer_ptr[u];
        assert(cmd.type < FUSE_CMD_COUNT && "[FUSE] WARN: INVALID TYPE IN COMMAND BUFFER");
        callback(canvas, &cmd, data);
    }
}

extern void
fuse_canvas_resize(FuseCanvas canvas, float w, float h)
{
    canvas->width = w;
    canvas->height = h;
}

extern void
fuse_canvas_pointer(FuseCanvas canvas, int pointer_state, float x, float y)
{
    canvas->pointer_state = pointer_state;
    canvas->pointer_x = x;
    canvas->pointer_y = y;
#ifdef FUSE_DEBUG
    const char *pstate[] = {
        [FUSE_POINTER_NONE] = "NONE",
        [FUSE_POINTER_PRESSED] = "PRESSED",
        [FUSE_POINTER_RELEASED] = "RELEASED",
        [FUSE_POINTER_ALT] = "ALT"
    }; 
    printf("[DEBUG] [FUSE] px=%f py=%f ps=%s\n", canvas->pointer_x, canvas->pointer_y, pstate[pointer_state]);
#endif
}

extern void
fuse_canvas_destroy(FuseCanvas canvas)
{
    free(canvas->memory);
    free(canvas);
}

extern bool
fuse_button(float x, float y, float width, float height, uint32_t selected, uint32_t hovered)
{
    size_t id = fuse__element();
    bool clicked = false;

    bool hot = FUSE_IS_HOVERED(x, y, width, height);

    if (hot) f__canvas->hot = id;

    /* are we the active component? */
    if (f__canvas->active == id) {
        if (f__canvas->pointer_state == FUSE_POINTER_RELEASED) {
            if (hot) clicked = true;
            f__canvas->active = 0;
        }
    } else if (hot && f__canvas->pointer_state == FUSE_POINTER_PRESSED) {
        f__canvas->active = id;
    }

    FuseCmd cmd = {
        .type = FUSE_CMD_RECT,
        .rect = {
            .color = (hot) ? hovered : selected,
            .x = x,
            .y = y,
            .w = width,
            .h = height,
        }
    };

    fuse__cmdbuffer_push(cmd);

    return clicked;
}

extern void
fuse_slider(float x, float y, float width, float height, uint32_t slider, uint32_t nob, float *nob_pos)
{
    size_t id = fuse__element();

    /* are we the hot component? */
    bool hot = FUSE_IS_HOVERED(x, y, width, height);

    if (hot) f__canvas->hot = id;

    bool is_vert = (width < height);
    float nob_width  = is_vert ? width : height;
    float nob_height = is_vert ? width : height;
    float track_len = is_vert ? (height - nob_height) : (width - nob_width);
    float p_off     = is_vert ? (f__canvas->pointer_y - (y + nob_height * 0.5f)) : (f__canvas->pointer_x - (x + nob_width * 0.5f));
    float val       = (track_len > 0.0f) ? (p_off / track_len) : 0.0f;

    /* calculate nob_pos from pointer */
    if (f__canvas->active == id) {
        if (f__canvas->pointer_state == FUSE_POINTER_RELEASED) {
            if (hot) { /* clicked */ }
            f__canvas->active = 0;
        }
        *nob_pos        = FUSE_MAX(0.0f, FUSE_MIN(1.0f, val));

    } else if (hot && f__canvas->pointer_state == FUSE_POINTER_PRESSED) {
        f__canvas->active = id;
        *nob_pos        = FUSE_MAX(0.0f, FUSE_MIN(1.0f, val));

    }

    /* clamp nob_pos */
    *nob_pos = FUSE_LIMIT(*nob_pos, 0.0f, 1.0f);

    float nob_x = is_vert ? 0.0f : (*nob_pos * (width - nob_width));
    float nob_y = is_vert ? (*nob_pos * (height - nob_height)) : 0.0f;

    FuseCmd cmd = {
        .type = FUSE_CMD_RECT,
        .rect = {
            .color = slider,
            .x = x,
            .y = y,
            .w = width,
            .h = height,
        }
    };

    FuseCmd nob_rect = {
        .type = FUSE_CMD_RECT,
        .rect = {
            .color = nob, 
            .x = x + nob_x,
            .y = y + nob_y,
            .w = nob_width,
            .h = nob_height,
        }
    };

    fuse__cmdbuffer_push(cmd);
    fuse__cmdbuffer_push(nob_rect);
}

extern int
fuse_tooltip(const char *text)
{
    FUSE_TODO;
    /* NOTE(vasco): this is supposed to add a tooltip when the last element was hovered...
     * will probably need to grab the last element's box?
     * I mean i need to know it's bounds and add an element that
     * shows up after being hovered over a few frames
     * so we will need to retain some data via the hashmap
     *
     * that's the plan for now
     */
    (void) text;
}
                                                                                    
static inline size_t
fuse__element()
{
    // we increment first because 0 is used for when no element is 
    // selected
    size_t id = ++f__canvas->current_id;  
    return id;
}

static inline void
fuse__cmdbuffer_push(FuseCmd cmd)
{
    if (f__canvas->cmdbuffer_head < f__canvas->cmdbuffer_size)
        f__canvas->cmdbuffer_ptr[f__canvas->cmdbuffer_head++] = cmd;
}

static void
fuse__cmdbuffer_print()
{
    for (size_t u = 0; u < f__canvas->cmdbuffer_head; u++) {
        FuseCmd cmd = f__canvas->cmdbuffer_ptr[u];
        assert(cmd.type < FUSE_CMD_COUNT && "[FUSE] WARN: INVALID TYPE IN COMMAND BUFFER");

        printf("CMD: %s - ", fuse__cmd_name(cmd.type));
        switch (cmd.type) {
            case FUSE_CMD_RECT: 
                printf("x=%f y=%f w=%f h=%f col=%x", cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.rect.color);
                break;
        }
        printf("\n");
    }
}

static const char*
fuse__cmd_name(uint8_t type)
{
    assert(type < FUSE_CMD_COUNT);
    return fuse_cmd_name[type];
}


extern bool
fuse_button_text(const char *text, float x, float y, float width, float height, uint32_t selected, uint32_t hovered)
{
    (void) text; (void) x; (void) y; (void) width; (void) height; (void) selected; (void) hovered;
    FUSE_TODO;
    return false;
}

extern bool
fuse_checkbox_text(const char *text, bool *value)
{
    (void) text; (void) value;
    FUSE_TODO;
    return false;
}

extern bool
fuse_element_is_hovered(const char *text)
{
    (void) text;
    FUSE_TODO;
    return false;
}

extern void
fuse_hovered(void (*op)(void*), void *data)
{
    (void) op; (void) data;
    FUSE_TODO;
}

extern void
fuse_hovered_clear()
{
    FUSE_TODO;
}

extern void
fuse_rotate_rad()
{
    FUSE_TODO;
}

extern void
fuse_rotate_deg()
{
    FUSE_TODO;
}

extern void
fuse_layout(FuseLayoutInfo info)
{
    (void) info;
    FUSE_TODO;
}

extern void
fuse_div_begin(float x, float y, float width, float height, uint32_t color)
{
    (void) x; (void) y; (void) width; (void) height; (void) color;
    FUSE_TODO;
}

extern void
fuse_div_end()
{
    FUSE_TODO;
}

extern void
fuse_flex_begin(float x, float y, float width, float height, uint32_t color)
{
    (void) x; (void) y; (void) width; (void) height; (void) color;
    FUSE_TODO;
}

extern void
fuse_flex_end()
{
    FUSE_TODO;
}

extern FuseStyle
fuse_style_save(const char *path)
{
    (void) path;
    FUSE_TODO;
    return NULL;
}

extern void
fuse_style_set(FuseStyle style)
{
    (void) style;
    FUSE_TODO;
}

extern void
fuse_style_load(const char *path)
{
    (void) path;
    FUSE_TODO;
}

extern void
fuse_window_begin(char *name, float width, float height, uint32_t border_color, uint32_t border_width)
{
    (void) name; (void) width; (void) height; (void) border_color; (void) border_width;
    FUSE_TODO;
}

extern void
fuse_window_end()
{
    FUSE_TODO;
}

extern void
fuse_grid_begin(uint32_t cols, uint32_t rows, const float *col_ratios, const float *row_ratios)
{
    (void) cols; (void) rows; (void) col_ratios; (void) row_ratios;
    FUSE_TODO;
}

extern void fuse_grid_next_row() { FUSE_TODO; }
extern void fuse_grid_next_col() { FUSE_TODO; }
extern void fuse_grid_prev_row() { FUSE_TODO; }
extern void fuse_grid_prev_col() { FUSE_TODO; }
extern void fuse_grid_next()     { FUSE_TODO; }
extern void fuse_grid_prev()     { FUSE_TODO; }

extern void
fuse_grid_goto(uint32_t col, uint32_t row)
{
    (void) col; (void) row;
    FUSE_TODO;
}

extern void fuse_grid_end() { FUSE_TODO; }

extern void fuse_3d_object()   { FUSE_TODO; }
extern void fuse_3d_point_at() { FUSE_TODO; }

extern int
fuse_animate_i(FuseAnimation anim, int i1, int i2)
{
    (void) anim; (void) i1; (void) i2;
    FUSE_TODO;
    return 0;
}

extern float
fuse_animate_f(FuseAnimation anim, float f1, float f2, float f, bool loop)
{
    (void) anim; (void) f1; (void) f2; (void) f; (void) loop;
    FUSE_TODO;
    return 0.0f;
}

#endif // _FUSE_C_
