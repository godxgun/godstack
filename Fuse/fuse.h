/* ===========================================================================   
 * FUSE - Zero Dependency UI Library - Copyright (c) 2025-2026 Vasco Alves
 * See LICENCE file for license info.
 *
 * DESCRIPTION:
 * A UI library that outputs a buffer of draw commands (or vertices) for
 * you to render via a desired backend. By default all components have
 * "fixed" positioning and fixed size because we assume the user has
 * more knowledge of the final layout than we do. Tweening and relative
 * positioning are achieved by running helper functions on the
 * parameters you want to tween / be relative.
 *
 * EXAMPLE:
 * float tween_y = fuse_tween_f(FUSE_ANIMATION_EASE_IN, 0.1, 0.3, 1, start);
 * float rel_x = fuse_percent_x(30.45);
 * if (fuse_button(rel_x, tween_y, 0.5, 0.3, 0xFFFF00FF, 0xFF0000FF)) {
 *     printf("pressed\n");
 * }
 *  
 * =========================================================================== */

#ifndef _FUSE_H_
#define _FUSE_H_

#define FUSE_MAJOR  "0"
#define FUSE_MINOR  "0"
#define FUSE_PATCH  "1"

/* CHANGE LOG 
 * 0.0.1 - @vasco - slider
 */


#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "allocators.h"
#include "fuse_backend.h"

/* NOTE(vasco):
 * It's important that we make the API really good and future-proof.
 * Functions should take as many arguments as possible.
 * We can add shorthands later.
 */

typedef struct fuse_canvas_t*   FuseCanvas; // Basically the main context of each "surface" you want to draw on. 
typedef struct fuse_window_t*   FuseWindow; // A floating window / panel to separate your UI.
typedef struct fuse_font_t*     FuseFont;
typedef struct fuse_style_t*    FuseStyle;  // Configurable style for your application.
typedef void(*FuseRendererCallback)(FuseCanvas, FuseCmd *cmd, void *data);
typedef void(*FuseOpCallback)(void *data);

typedef struct FuseLayoutInfo {
    float min_width, min_height;
    float max_width, max_height;
    float gap;
    uint8_t direction;
} FuseLayoutInfo;

typedef enum {
    FUSE_ANIMATION_LINEAR = 0,
    FUSE_ANIMATION_EASE_IN,
    FUSE_ANIMATION_EASE_OUT
} FuseAnimation;

typedef enum {
    FUSE_POINTER_NONE = 0,
    FUSE_POINTER_RELEASED,
    FUSE_POINTER_PRESSED,
    FUSE_POINTER_ALT
} FusePointerState;

typedef enum {
    FUSE_DIRECTION_ROW = 0,
    FUSE_DIRECTION_COLUMN,
} FuseDirection;

/* canvas creation and drawing */
extern FuseCanvas  fuse_canvas_create(size_t memory_limit, FuseStyle base_style); // Canvas initialization. Contains all the context needed to draws. Doesn't care about what you're drawing to since the library emits normalizer coordinates. 
extern void        fuse_canvas_set(FuseCanvas); // Changes the canvas that the rest of the API outputs to by default. Normally you will only be drawing to a single instance at a time.
extern void        fuse_canvas_debug(FuseCanvas, bool); // Enable debugging for a specific canvas.
extern void        fuse_canvas_clear(FuseCanvas canvas); // Clears canvas to begin drawing the next frame.
extern void        fuse_canvas_draw(FuseCanvas, FuseRendererCallback, void* data); // Sends all the draw commands to the renderer callback function. 
extern void        fuse_canvas_resize(FuseCanvas, float w, float h); // Updates the width and height of the active canvas.
extern void        fuse_canvas_pointer(FuseCanvas, int pointer_state, float x, float y); // Update pointer state on active canvas.
extern void        fuse_canvas_destroy(FuseCanvas); // Clear all the context and free all the memory.

/* components */
extern bool        fuse_button(float x, float y, float width, float height, uint32_t selected, uint32_t hovered); // Base button component. Returns true if clicked on that frame.
extern void        fuse_slider(float x, float y, float width, float height, uint32_t slider, uint32_t nob, float *nob_pos);
extern bool        fuse_button_text(const char *text, float x, float y, float width, float height, uint32_t selected, uint32_t hovered);
extern bool        fuse_checkbox_text(const char *text, bool *value);
                                                                                                            
/* conditionals */
extern bool        fuse_element_is_hovered(const char *text);

/* hover callbacks */
extern void        fuse_hovered(void (*op)(void*), void *data); // Register hover callback for subsequent elements.
extern void        fuse_hovered_clear(); // Clear hover callback.


/* operations */
extern int         fuse_tooltip(const char *text); // Adds tooltip to the NEXT element.
extern void        fuse_rotate_rad(); // Rotate next element by x radians 
extern void        fuse_rotate_deg(); // Rotate next element by x degrees.
extern float       fuse_percent_x(float p); // Get percentage of width of the parent. Left of the secreen is 0%.
extern float       fuse_percent_y(float p); // Get percentage of height of the screen. Top of the screen is 0%.
extern float       fuse_tween_f(FuseAnimation anim, float f1, float f2, float duration, uint64_t time_ns); // Interpolates between floats. Takes in the animation duration in seconds and the current time in nanoseconds.
extern int         fuse_tween_i(FuseAnimation anim, int i1, int i2, float duration, uint64_t time_ns); // Interpolates between integers. Takes in the animation duration in seconds and the current time in nanoseconds.
extern uint32_t    fuse_tween_color(FuseAnimation anim, uint32_t c1, uint32_t c2, float duration, uint64_t time_ns);  // Interpolates between colors. Takes in the animation duration in seconds and the current time in nanoseconds.

/* layouting */
#define FUSE(...)  fuse_layout((FuseLayoutInfo){ __VA_ARGS__ })
#define FUSE_LAYOUT_DIRECTION(dir) fuse_layout((FuseLayoutInfo){ .direction = (dir) })
extern void        fuse_layout(FuseLayoutInfo);
extern void        fuse_div_begin(float x, float y, float width, float height, uint32_t color);
extern void        fuse_div_end();
extern void        fuse_flex_begin(float x, float y, float width, float height, uint32_t color);
extern void        fuse_flex_end();

/* 3d */
extern void        fuse_3d_object();
extern void        fuse_3d_point_at();

/* do we need this? im pretty sure the user can figure it out by using a step function
 * you know like the Heaviside step function u(t) where u(t) = 1, t >= 0, u(t) = 0 otherwise
 *
 *  animation function a(t) = f(t - t0) * (u(t1) - u(t0))
 *
 *  in fact we already have the tween functions, all we need it u(t) and boom
 */
extern int         fuse_animate_i(FuseAnimation, int i1, int i2);
extern float       fuse_animate_f(FuseAnimation, float f1, float f2, float f, bool loop);


#endif // _FUSE_H_
