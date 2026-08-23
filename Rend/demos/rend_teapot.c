
#include "teapot.h"

#include "../rend.h"
#include "../rend.c"
#include <stddef.h>

#define PODIUM_IMPLEMENTATION
#define P_MODULE_MATH
#define P_MODULE_VULKAN
#include "podium.h"

int main() {

    const char *texture_path = "wal.jpg";
    if (!p_file_exists(texture_path)) {
        texture_path = "demos/wal.jpg";
    }
    typedef struct CoolPushConstants {
        P_Mat4 MVP;
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

    P_Mat4 view = p_mat4_translate((Vec3f){0.0f, 0.0f, -3.0f});
    P_Mat4 proj = p_mat4_perspective(P_PI_HALF, 800.0f / 600.0f, 0.1f, 100.0f);
    proj.m[1 * 4 + 1] *= -1.0f; /* Vulkan clip space Y inversion fix */

    bool running = true;
    uint64_t frame_count = 0;
    float angle = 0;

    bool pointer_down = false;
    uint32_t last_pointer_x = 0;
    uint32_t last_pointer_y = 0;

    uint16_t frames = 3;
    while (running) {

        P_Event ev;
        while (p_window_poll_event(win, &ev)) {
            if (ev.type == P_EVENT_WINDOW_CLOSE) {
                running = false;
                break;
            }
            if (ev.type == P_EVENT_KEY_DOWN) {
                switch (ev.key.key) {
                    case P_KEY_W:
                        view = p_mat4_translate_by(view, (Vec3f){0.0f, 0.0f, 0.1f});
                        break;
                    case P_KEY_S:
                        view = p_mat4_translate_by(view, (Vec3f){0.0f, 0.0f, -0.1f});
                        break;
                    case P_KEY_A:
                        view = p_mat4_translate_by(view, (Vec3f){0.1f, 0.0f, 0.0f});
                        break;
                    case P_KEY_D:
                        view = p_mat4_translate_by(view, (Vec3f){-0.1f, 0.0f, 0.0f});
                        break;

                }
                break;
            }
            if (ev.type == P_EVENT_POINTER) {
                if (ev.pointer.state == P_POINTER_PRESSED && ev.pointer.button == 1) {
                    pointer_down = true;
                    last_pointer_x = ev.pointer.x;
                    last_pointer_y = ev.pointer.y;
                } else if (ev.pointer.state == P_POINTER_RELEASED && ev.pointer.button == 1) {
                    pointer_down = false;
                } else if (ev.pointer.state == P_POINTER_MOVED) {
                    if (pointer_down) {
                        float dx = (float)((int)ev.pointer.x - (int)last_pointer_x);
                        float dy = (float)((int)ev.pointer.y - (int)last_pointer_y);

                        float sensitivity = 0.005f;
                        float yaw = dx * sensitivity;
                        float pitch = dy * sensitivity;

                        view = p_mat4_rotate_y_by(view, yaw);
                        view = p_mat4_mul(p_mat4_rotate_x(pitch), view);

                        last_pointer_x = ev.pointer.x;
                        last_pointer_y = ev.pointer.y;
                    }
                }
            }
        }

        angle += 0.05;

        P_Mat4 model = p_mat4_identity();
        model = p_mat4_rotate_y_by(model, angle * 0.5f);

        P_Mat4 mvp = p_mat4_mul(proj, p_mat4_mul(view, model));

        CoolPushConstants pc = {
            .MVP = mvp,
        };

        if (rend_renderer_frame_begin(renderer)) {

            rend_renderer_render_pass_begin(renderer, 0.5, 0.5, 0.5, 0.0); {
                rend_pipeline_bind(basic_pipeline);
                rend_pipeline_push_constants(basic_pipeline, &pc, sizeof pc);
                rend_pipeline_bind_texture(basic_pipeline, &texture, 1, 0);
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

    return 0;
}
