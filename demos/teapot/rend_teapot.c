#define PEAK_IMPLEMENTATION
#include "../Rend/rend.h"
#include "../Rend/rend.c"
#include "teapot.h"
#define GRIT_IMPLEMENTATION
#include "../../Grit/grit.h"
#include <stddef.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static uint8_t *
load_spv(const char *a, const char *b, unsigned long *size)
{
    uint8_t *p = peak_file_alloc(a, size);
    if (p) return p;
    return peak_file_alloc(b, size);
}

int main() {

    if (!peak_init()) {
        PFATAL("Failed to init Peak!");
        return 1;
    }

    PeakWindow win = peak_window_open("teapot", 800, 600, 0);
    if (!win.running) {
        PFATAL("Failed to open a window!");
        return 1;
    }

    RendBindingInfo bind_info = {0};
    RendRenderer renderer;

    /* basic.slang: set 0 binding 0 = ubos[3], binding 1 = test[3] */
    bind_info.ubo_bindings[0] = 0;
    bind_info.ubo_array_sizes[0] = 3;
    bind_info.ubo_binding_count = 1;
    bind_info.texture_bindings[0] = 1;
    bind_info.texture_array_sizes[0] = 3;
    bind_info.texture_binding_count = 1;
    renderer = rend_renderer_create(&win, REND_BACKEND_AUTO, NULL, true, &bind_info);
    if (!renderer) {
        PFATAL("Failed to create renderer!");
        peak_window_close(&win);
        peak_quit();
        return 1;
    }

    const char *texture_path = "wal.jpg";
    if (!peak_file_exists(texture_path)) {
        texture_path = "demos/teapot/wal.jpg";
        if (!peak_file_exists(texture_path)) {
            texture_path = "demos/wal.jpg";
        }
    }
    typedef struct CoolPushConstants {
        float MVP[16];
    } CoolPushConstants;

    typedef struct CoolVertex {
        float pos[3];
        float uv[2];
    } CoolVertex;

    CoolVertex cool_array[TEAPOT_VERTEX_COUNT];

    for (uint32_t i = 0; i < TEAPOT_VERTEX_COUNT; ++i) {
        cool_array[i].pos[0] = teapot_positions[i][0];
        cool_array[i].pos[1] = teapot_positions[i][1];
        cool_array[i].pos[2] = teapot_positions[i][2];

        float u = teapot_positions[i][0] / teapot_positions[i][2];
        float v = teapot_positions[i][1] / teapot_positions[i][2];

        cool_array[i].uv[0] = u;
        cool_array[i].uv[1] = v;
    }

    size_t vert_size = TEAPOT_VERTEX_COUNT * sizeof *cool_array;
    size_t ind_size = TEAPOT_INDEX_COUNT * sizeof *teapot_indices;

    RendBuffer vert_buf = rend_buffer_create(renderer, vert_size, REND_BUFFER_VERTEX, true);
    RendBuffer ind_buf = rend_buffer_create(renderer, ind_size, REND_BUFFER_INDEX, true);
    rend_buffer_write(renderer, &vert_buf, cool_array, vert_size, 0);
    rend_buffer_write(renderer, &ind_buf, teapot_indices, ind_size, 0);

    int tex_w, tex_h, tex_channels;
    unsigned char *pixels = stbi_load(texture_path, &tex_w, &tex_h, &tex_channels, 4);
    RendTexture texture = rend_texture_create(renderer, tex_w, tex_h, 1, 0, 1, REND_FORMAT_R8G8B8A8_SRGB);
    if (pixels) {
        rend_texture_copy_data(renderer, &texture, pixels, (size_t)tex_w * tex_h * 4);
        stbi_image_free(pixels);
    }

    unsigned long basic_vert_bytes = 0;
    unsigned long basic_frag_bytes = 0;
    uint8_t *basic_vert = load_spv("basic.vert.spv", "demos/teapot/basic.vert.spv", &basic_vert_bytes);
    uint8_t *basic_frag = load_spv("basic.frag.spv", "demos/teapot/basic.frag.spv", &basic_frag_bytes);

    RendPipeline basic_pipeline = rend_pipeline_create_graphics_spirv(
            renderer,
            basic_vert, basic_vert_bytes,
            basic_frag, basic_frag_bytes,
            &(RendVertexBinding){ .binding = 0, .stride = sizeof(CoolVertex), .input_rate = REND_INPUT_RATE_VERTEX }, 1,
            (RendVertexAttributes[]){
            { .binding = 0, .location = 0, .format = REND_FORMAT_3_SFLOAT32, .offset = offsetof(CoolVertex, pos) },
            { .binding = 0, .location = 1, .format = REND_FORMAT_2_SFLOAT32, .offset = offsetof(CoolVertex, uv) }
            }, 2,
            &(RendPushConstantInfo){ .offset = 0, .size = sizeof(CoolPushConstants) }, 1,
            REND_POLYGON_MODE_FILL,
            REND_CULL_MODE_NONE,
            REND_TOPOLOGY_TRIANGLE_STRIP,
            REND_FORMAT_UNDEFINED,
            true
            );

    free(basic_vert);
    free(basic_frag);
    if (!basic_pipeline) {
        PFATAL("Failed to create teapot pipeline!");
        return 1;
    }
    rend_pipeline_bind_texture(basic_pipeline, &texture, 1, 0);

    float view[16], proj[16];
    float eye[3] = {0.0f, 0.0f, -3.0f};
    grit_mat4_translate(view, eye);
    grit_mat4_perspective(proj, GRIT_PI_HALF, 800.0f / 600.0f, 0.1f, 100.0f);
    proj[5] *= -1.0f; /* Vulkan clip space Y inversion fix */

    bool running = true;
    float angle = 0;

    bool pointer_down = false;
    float last_pointer_x = 0;
    float last_pointer_y = 0;

    while (running) {

        PeakEvent ev;
        while (peak_window_epoll(&win, &ev)) {
            if (ev.type == PEAK_EVENT_WINDOW_CLOSE) {
                running = false;
                break;
            }
            if (ev.type == PEAK_EVENT_KEY_DOWN) {
                switch (ev.key.key) {
                    case PEAK_KEY_W: {
                        float d[3] = {0.0f, 0.0f, 0.1f};
                        grit_mat4_translate_by(view, d);
                    } break;
                    case PEAK_KEY_S: {
                        float d[3] = {0.0f, 0.0f, -0.1f};
                        grit_mat4_translate_by(view, d);
                    } break;
                    case PEAK_KEY_A: {
                        float d[3] = {0.1f, 0.0f, 0.0f};
                        grit_mat4_translate_by(view, d);
                    } break;
                    case PEAK_KEY_D: {
                        float d[3] = {-0.1f, 0.0f, 0.0f};
                        grit_mat4_translate_by(view, d);
                    } break;
                    default: break;
                }
                break;
            }
            if (ev.type == PEAK_EVENT_POINTER) {
                if (ev.pointer.state == PEAK_POINTER_PRESSED && ev.pointer.type == PEAK_POINTER_LEFT) {
                    pointer_down = true;
                    last_pointer_x = ev.pointer.x;
                    last_pointer_y = ev.pointer.y;
                } else if (ev.pointer.state == PEAK_POINTER_RELEASED && ev.pointer.type == PEAK_POINTER_LEFT) {
                    pointer_down = false;
                } else if (ev.pointer.state == PEAK_POINTER_MOVED) {
                    if (pointer_down) {
                        float dx = ev.pointer.x - last_pointer_x;
                        float dy = ev.pointer.y - last_pointer_y;

                        float sensitivity = 0.005f;
                        float yaw = dx * sensitivity;
                        float pitch = dy * sensitivity;

                        grit_mat4_rotate_y_by(view, yaw);
                        {
                            float rx[16];
                            int i;
                            grit_mat4_rotate_x(rx, pitch);
                            grit_mat4_mul(rx, view);
                            for (i = 0; i < 16; i++) view[i] = rx[i];
                        }

                        last_pointer_x = ev.pointer.x;
                        last_pointer_y = ev.pointer.y;
                    }
                }
            }
        }

        angle += 0.05;

        float model[16], vm[16], mvp[16];
        int i;
        grit_mat4_identity(model);
        grit_mat4_rotate_y_by(model, angle * 0.5f);

        for (i = 0; i < 16; i++) vm[i] = view[i];
        grit_mat4_mul(vm, model);
        for (i = 0; i < 16; i++) mvp[i] = proj[i];
        grit_mat4_mul(mvp, vm);

        CoolPushConstants pc;
        for (i = 0; i < 16; i++) pc.MVP[i] = mvp[i];

        if (rend_renderer_frame_begin(renderer)) {

            rend_renderer_render_pass_begin(renderer, 0.5, 0.5, 0.5, 0.0); {
                rend_pipeline_bind(basic_pipeline);
                rend_pipeline_push_constants(basic_pipeline, &pc, sizeof pc);
                rend_pipeline_bind_vertex_buffer(basic_pipeline, 0, vert_buf, 0);
                rend_pipeline_bind_index_buffer(basic_pipeline, ind_buf, 0, REND_INDEX_UINT16);
                rend_pipeline_draw_indexed(basic_pipeline, TEAPOT_INDEX_COUNT, 0, 0, 1);
            } rend_renderer_render_pass_end(renderer);

            rend_renderer_frame_end(renderer, NULL);
        } 

    }

    rend_texture_destroy(renderer, &texture);
    rend_buffer_destroy(&vert_buf);
    rend_buffer_destroy(&ind_buf);
    rend_renderer_destroy(renderer);

    peak_window_close(&win);
    peak_quit();
    rend_quit();

    return 0;
}
