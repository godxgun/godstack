#include "rend.h"
#include "rend.c"
#include "fuse.h"
#include "fuse.c"
#include <stddef.h>
#include <stdio.h>

/* has to be included last because rend will ask for certain modules */
#define PODIUM_IMPLEMENTATION
#include "podium.h"

typedef struct { float x, y; } FuseVec2;
typedef struct { float x, y, z, w; } FuseVec4;

#pragma pack(push, 1)
typedef struct {
    FuseVec2 pos;
    FuseVec4 color;
} FuseVertex;
#pragma pack(pop)

static FuseVertex fuse_renderer_data[1024];
static size_t fuse_renderer_idx = 0;

struct data_t { float offset_x; float padding = 0 };
struct data_t button_data = { .offset_x = 0, .padding = 0};

void stretch_button_op(void *data) {
    struct data_t* ptr = (struct data_t*) data;
    data->offset_x += 10;
    data->padding += 10;
}


static inline FuseVec4
fuse__unpack_color(uint32_t c) 
{
    FuseVec4 v;
    v.x = (float) ((c >> 24) & 0xFF) / 255.0f; // Red
    v.y = (float) ((c >> 16) & 0xFF) / 255.0f; // Green
    v.z = (float) ((c >> 8)  & 0xFF) / 255.0f; // Blue
    v.w = (float) ( c        & 0xFF) / 255.0f; // Alpha
    return v;
}

void fuse_handler(FuseCanvas canvas, FuseCmd *cmd, void *data) {
    switch (cmd->type) {
        case FUSE_CMD_RECT:
            FuseRect rect = cmd->rect;
            FuseVec4 color = fuse__unpack_color(rect.color);
            FuseVertex c0 = {
                .pos   = { rect.x, rect.y },
                .color = color
            };

            FuseVertex c1 = {
                .pos   = { rect.x + rect.w, rect.y },
                .color = color
            };

            FuseVertex c2 = {
                .pos   = { rect.x + rect.w, rect.y + rect.h },
                .color = color
            };

            FuseVertex c3 = {
                .pos   = { rect.x, rect.y + rect.h },
                .color = color
            };

            fuse_renderer_data[fuse_renderer_idx++] = c0;
            fuse_renderer_data[fuse_renderer_idx++] = c1;
            fuse_renderer_data[fuse_renderer_idx++] = c2;

            fuse_renderer_data[fuse_renderer_idx++] = c2;
            fuse_renderer_data[fuse_renderer_idx++] = c3;
            fuse_renderer_data[fuse_renderer_idx++] = c0;
            break;
    }
}

#define SCREEN_WIDTH    800
#define SCREEN_HEIGHT   600

int main() {

    P_Window window;
    if (!p_window_open(&window, 800, 600, "fuse_demo")) {
        PFATAL("Failed to open window!");
        return 1;
    }

    if (!rend_init(REND_BACKEND_AUTO)) {
        PFATAL("Failed to initialize backend!");
        return 1;
    }

    RendRenderer renderer = rend_renderer_create(&window, true);
    RendShader vert = rend_shader_from_spv_path(REND_SHADER_VERTEX, "fuse.vert.spv");
    RendShader frag = rend_shader_from_spv_path(REND_SHADER_FRAG,   "fuse.frag.spv");

    RendPipelineConfig ui_cfg = rend_pipeline_config_create(
        REND_TOPOLOGY_TRIANGLE_LIST,
        REND_POLYGON_MODE_FILL,
        REND_CULL_MODE_BACK);
    rend_pipeline_config_depth_test(ui_cfg, false);

    /* configure vertex input */
    int b0 = rend_pipeline_config_vertex_binding_add(ui_cfg, sizeof (FuseVertex), REND_INPUT_RATE_VERTEX);
    rend_pipeline_config_vertex_attribute_add(ui_cfg, b0, 0, REND_FORMAT_R32G32_SFLOAT, offsetof(FuseVertex, pos));
    rend_pipeline_config_vertex_attribute_add(ui_cfg, b0, 1, REND_FORMAT_R32G32B32A32_SFLOAT, offsetof(FuseVertex, color));
    rend_pipeline_config_resource_binding_add(ui_cfg, 0, 0, REND_BUFFER_UNIFORM, REND_SHADER_VERTEX, 1);

    /* add the shaders */
    rend_pipeline_config_shader_add(ui_cfg, vert);
    rend_pipeline_config_shader_add(ui_cfg, frag);

    RendPipeline ui_pipeline = rend_pipeline_create(renderer, ui_cfg);

    RendResourceSet ui_resources = rend_resource_set_create(renderer, ui_pipeline, 0);

    FuseCanvas canvas = fuse_canvas_create(1024, NULL);

    u64 frame_t = NANOS_PER_SEC / 60;

    bool running = true;
    P_Event ev;
    float pev = 0, px = 0, py = 0;

    float nob = 0.3;

    float target_screen[2] = { SCREEN_WIDTH, SCREEN_HEIGHT };
    rend_resource_set_write_buffer(ui_resources, 0, REND_BUFFER_UNIFORM, target_screen, 2 * sizeof *target_screen, 0);

    float real_screen_width = 0;
    float real_screen_heigth = 0;

    enum UI_State {
        UI_START_MENU,
        UI_OPTIONS,
    };

    int ui_state = UI_START_MENU;

    while (running) {

        u64 start = p_get_time();
        
        pev = 0;
        /* event loop */
        while (p_window_poll_event(&window, &ev)) {
            if (ev.type == P_EVENT_WINDOW_CLOSE) {
                running = false;
                break;
            }

            switch (ev.type) {
                case P_EVENT_WINDOW_RESIZE:
                    real_screen_width = ev.resize.width;
                    real_screen_heigth = ev.resize.height;
                    break;
                case P_EVENT_POINTER:
                    px = ev.pointer.x;
                    py = ev.pointer.y;
                    switch (ev.pointer.state) {
                        case P_POINTER_PRESSED:
                            if (ev.pointer.button == 1 || ev.pointer.button == 0)
                                pev = FUSE_POINTER_PRESSED;
                            break;
                        case P_POINTER_RELEASED:
                            if (ev.pointer.button == 1 || ev.pointer.button == 0)
                                pev = FUSE_POINTER_RELEASED;
                            break;
                        default:
                        case P_POINTER_MOVED:
                            break;
                    }

                    fuse_canvas_pointer(pev, px * SCREEN_WIDTH / real_screen_width, py * SCREEN_HEIGHT / real_screen_heigth);
                    break;
            }
        }

        /* Clears the Screen */
        fuse_canvas_clear(canvas, 0x000000); 

        /* Inside a div, children no longer have fixed positions,
         * instead x, y represent an offset so that they can still be animated!
         * Width and height represent min width and min height instead! */
        if (ui_state == UI_START_MENU) {
            // NOTE: brackets for legibility only
            FUSE_LAYOUT_DIRECTION(FUSE_DIRECTION_COLUMN);
            fuse_div_begin(0, 0, SCREEN_WIDTH / 4, SCREEN_HEIGHT, 0xFFFFFFaa);
            { 

                fuse_hovered(stretch_button_op, data);

                if (fuse_button_text("Start New Game", button_data.offset_x, 0, 100, 0, 0xFF0000FF, 0x000000FF)) {
                    printf("start new game");
                }


                if (fuse_button_text("Options", 0, 0, 100, 0, 0xFF0000FF, 0x000000FF)) {
                    ui_state = UI_OPTIONS;
                }


                if (fuse_button_text("Credits", 0, 0, 100, 0, 0xFF0000FF, 0x000000FF)) {
                    printf("awesome credits");
                }

                if (fuse_button_text("Quit Game")) {
                    running = false;
                    break;
                }

                fuse_hovered_clear();

            }
            fuse_div_end();

            /* back to global layouting */
            fuse_3d_object();
            fuse_3d_point_at();


        } else if (ui_state == UI_OPTIONS) {

            /* Inside a flex the same x,y as offsets and width and height as minimums rules apply
             * The difference is that a flex is flexible, by default it does not have a fixed size
             * but you may specify minimums and maximus for it to work around. 
             */

            FUSE(
                    .max_width = 300,
                    .max_height = 600,
                    .direction = FUSE_DIRECTION_COLUMN,
                    .gap = 32
            );

            fuse_flex_begin(0, 0, SCREEN_WIDTH / 4, SCREEN_HEIGHT, 0xFFFFFFaa);
            {
                if (fuse_checkbox_text("VSYNC", &vsync)) {
                    vsync = ~vsync;
                }
            }
        }

        fuse_slider(100, 200, 300, 600, 0xFFFF00FF, 0xFF0000FF, &nob);

        fuse_canvas_draw(canvas, fuse_handler, NULL);

        /* call renderer */
        if (rend_renderer_frame_begin(renderer)) {
            rend_pipeline_bind(ui_pipeline);
            rend_resource_set_bind(ui_pipeline, 0, ui_resources);

            rend_pipeline_push_data(ui_pipeline, fuse_renderer_data, fuse_renderer_idx, sizeof *fuse_renderer_data);
            rend_pipeline_draw(ui_pipeline);

            rend_renderer_frame_end(renderer, NULL);
        }

        fuse_renderer_idx = 0;

        u64 elapsed = p_get_time() - start;

        /* the renderer should already be forcing vsync */
        if (elapsed < frame_t)
            p_sleep_ns(frame_t - elapsed);
    }


    fuse_canvas_destroy(canvas);

    rend_quit();

    p_window_close(&window);
    return 0;
}
