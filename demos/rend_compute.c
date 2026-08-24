#include <string.h>

#ifdef DEBUG_MEMORY
#define malloc(size)  peak_debug_malloc_impl((size), __FILE__, __LINE__, __func__)
#define realloc(ptr, size) peak_debug_realloc_impl((ptr), (size), __FILE__, __LINE__, __func__)
#define free(ptr)     peak_debug_free_impl((ptr), __FILE__, __LINE__, __func__)
#endif

#define PEAK_IMPLEMENTATION
#include "../Rend/rend.h"
#include "../Rend/rend.c"
#include <stddef.h>

float delta = 0;
PeakWindow win;
PeakEvent ev;

bool vsync = true;

#define PARTICLE_COUNT 1024 * 10
#define GRID_WIDTH  512
#define GRID_HEIGHT 512

typedef struct {
    uint64_t particles;
    uint64_t src_grid;
    uint64_t dst_grid;
    uint32_t particle_count;
    uint32_t width;
    uint32_t height;
    float delta_time;
    uint32_t render_mode;
} PushConstants;

typedef struct Particle {
    float pos[2];
    float vel[2];
} Particle;

uint32_t pcg_hash(uint32_t seed) {
    uint32_t state = seed * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float cool_beans(uint32_t *seed) {
    *seed = pcg_hash(*seed);
    return (float) *seed / 4294967295.0;
}

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

    win = peak_window_open("demo", 800, 600, 0);
    if (!win.running) {
        PFATAL("Failed to open a window!");
        return 1;
    }

    // bindless setup
    RendBindingInfo bind_info = {0};
    RendRenderer renderer = rend_renderer_create(&win, REND_BACKEND_AUTO, NULL, vsync, &bind_info);
    if (!renderer) {
        PFATAL("Failed to create renderer!");
        peak_window_close(&win);
        peak_quit();
        return 1;
    }

    unsigned long particles_bytes = 0;
    uint8_t *particles_shader = load_spv("particle.spv", "demos/compute/particle.spv", &particles_bytes);

    unsigned long sand_bytes = 0;
    uint8_t *sand_shader = load_spv("sand.spv", "demos/compute/sand.spv", &sand_bytes);

    unsigned long vert_size = 0;
    uint8_t *vert = load_spv("vert.spv", "demos/compute/vert.spv", &vert_size);

    unsigned long frag_size = 0;
    uint8_t *frag = load_spv("frag.spv", "demos/compute/frag.spv", &frag_size);

    uint32_t seed = 0xC0FFEE;

    Particle *initial_particles = malloc(PARTICLE_COUNT * sizeof *initial_particles);
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        initial_particles[i].pos[0] = cool_beans(&seed) * 2.0f - 1.0f;
        initial_particles[i].pos[1] = cool_beans(&seed) * 2.0f - 1.0f;
        initial_particles[i].vel[0] = cool_beans(&seed) * 0.4f - 0.2f;
        initial_particles[i].vel[1] = cool_beans(&seed) * 0.4f - 0.2f;
    }

    size_t buffer_size = PARTICLE_COUNT * sizeof(Particle);
    RendBuffer particle_buf = rend_buffer_create(renderer, buffer_size, REND_BUFFER_STORAGE, true);
    rend_buffer_write(renderer, &particle_buf, initial_particles, buffer_size, 0);

    size_t grid_buffer_size = GRID_WIDTH * GRID_HEIGHT * sizeof(uint32_t);
    uint32_t *initial_grid = calloc(GRID_WIDTH * GRID_HEIGHT, sizeof(uint32_t));

    for (int y = 0; y < 100; y++) {
        for (int x = 150; x < 362; x++) {
            initial_grid[y * GRID_WIDTH + x] = 1; // 1 = sand
        }
    }

    RendBuffer src_buf = rend_buffer_create(renderer, GRID_WIDTH * GRID_HEIGHT * sizeof(uint32_t), REND_BUFFER_STORAGE, false);
    RendBuffer dst_buf = rend_buffer_create(renderer, GRID_WIDTH * GRID_HEIGHT * sizeof(uint32_t), REND_BUFFER_STORAGE, false);
    rend_buffer_write(renderer, &src_buf, initial_grid, grid_buffer_size, 0);
    rend_buffer_write(renderer, &dst_buf, initial_grid, grid_buffer_size, 0);

    uint64_t particles_address = rend_buffer_address(&particle_buf);

    RendBuffer *src_grid = &src_buf;
    RendBuffer *dst_grid = &dst_buf;

    RendPushConstantInfo pc = { .offset = 0, .size = sizeof(PushConstants) };
    RendPipeline particle_compute = rend_pipeline_create_compute_spirv(renderer, particles_shader, particles_bytes, &pc, 1);
    RendPipeline sand_compute = rend_pipeline_create_compute_spirv(renderer, sand_shader, sand_bytes, &pc, 1);
    free(particles_shader);
    free(sand_shader); 
                        
    RendPipeline graphics_pipeline = rend_pipeline_create_graphics_bindless_spirv(renderer, vert, vert_size, frag, frag_size, &pc, 1, REND_POLYGON_MODE_FILL, REND_CULL_MODE_NONE, REND_TOPOLOGY_TRIANGLE_LIST, 0, true);

    free(vert);
    free(frag);

    float dt = 1.0f / 60.0f; 
    uint64_t dt_ns = (uint64_t) (dt * NANOS_PER_SEC);

    uint32_t mode = 2;
    while (mode > 0) {

        uint64_t start = peak_get_time();

        PeakEvent ev;
        while (peak_window_epoll(&win, &ev)) {
            if (ev.type == PEAK_EVENT_WINDOW_CLOSE) {
                mode -= 1;
                break;
            }
        }

        if (rend_renderer_frame_begin(renderer)) {

            PushConstants pc = {0};

            if (mode == 2) {
                /*
                 * Sand Simulation 
                 */
                pc.delta_time   = dt;
                pc.width        = GRID_WIDTH;
                pc.height       = GRID_HEIGHT;
                pc.render_mode  = 1;
                pc.src_grid     = src_grid->gpu_address;
                pc.dst_grid     = dst_grid->gpu_address;

                rend_pipeline_bind(sand_compute);
                rend_pipeline_push_constants(sand_compute, &pc, sizeof(pc));
                
                uint32_t x = (GRID_WIDTH  + 15) / 16;
                uint32_t y = (GRID_HEIGHT + 15) / 16;
                rend_pipeline_dispatch(sand_compute, x, y, 1);

                RendBuffer *temp = src_grid;
                src_grid = dst_grid;
                dst_grid = temp;

                memset(src_grid->mapped_memory, 0, grid_buffer_size);

            } else {
                /*
                 * Particles Simulation 
                 */
                pc.particles      = particles_address;
                pc.delta_time     = dt;
                pc.particle_count = PARTICLE_COUNT;
                pc.render_mode    = 0;

                rend_pipeline_bind(particle_compute);
                rend_pipeline_push_constants(particle_compute, &pc, sizeof(pc));
                rend_pipeline_dispatch(particle_compute, (PARTICLE_COUNT + 255) / 256, 1, 1);
            }

            rend_renderer_render_pass_begin(renderer, 0.5f, 0.5f, 0.5f, 0.0f); {
                rend_pipeline_bind(graphics_pipeline);
                rend_pipeline_push_constants(graphics_pipeline, &pc, sizeof(pc));

                uint32_t vertex_count = (mode == 2) ? (6 * GRID_WIDTH * GRID_HEIGHT) : (6 * PARTICLE_COUNT);
                rend_pipeline_draw(graphics_pipeline, vertex_count, 1);

            } rend_renderer_render_pass_end(renderer);

            rend_renderer_frame_end(renderer, NULL);
        } 

        uint64_t delta_ns = peak_get_time() - start;
        if (delta_ns < dt_ns) {
            peak_sleep_ns(dt_ns - delta_ns);
        }
    }

    free(initial_particles);
    free(initial_grid);

    rend_buffer_destroy(&particle_buf);
    rend_buffer_destroy(&src_buf);
    rend_buffer_destroy(&dst_buf);

    rend_renderer_destroy(renderer);
    rend_quit();

    peak_window_close(&win);
    peak_quit();

#if DEBUG_MEMORY
    peak_debug_memory_report();
#endif
    return 0;
}
