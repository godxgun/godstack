static inline void peak__put_pixel(int x, int y, uint32_t color);
static inline uint32_t peak__lerp_color(uint32_t c0, uint32_t c1, float t);
static inline float peak__edge_cross(float ax, float ay, float bx, float by, float cx, float cy);

static uint32_t *peak_window_buffer;
static size_t peak_window_width;
static size_t peak_window_height;
static int peak_clip_x;
static int peak_clip_y;
static size_t peak_clip_width;
static size_t peak_clip_height;

void
peak__put_pixel(int x, int y, uint32_t color) 
{
    if (x < peak_clip_x || x >= (int)(peak_clip_x + peak_clip_width) ||
        y < peak_clip_y || y >= (int)(peak_clip_y + peak_clip_height)) {
        return;
    }
    if (x < 0 || x >= (int)peak_window_width || y < 0 || y >= (int)peak_window_height) {
        return;
    }
    peak_window_buffer[y * peak_window_width + x] = color;
}

uint32_t
peak__lerp_color(uint32_t c0, uint32_t c1, float t) 
{
    if (t <= 0.0f) return c0;
    if (t >= 1.0f) return c1;

    uint32_t a0 = (c0 >> 24) & 0xFF, a1 = (c1 >> 24) & 0xFF;
    uint32_t r0 = (c0 >> 16) & 0xFF, r1 = (c1 >> 16) & 0xFF;
    uint32_t g0 = (c0 >> 8)  & 0xFF, g1 = (c1 >> 8)  & 0xFF;
    uint32_t b0 = c0 & 0xFF,        b1 = c1 & 0xFF;

    uint32_t a = (uint32_t)(a0 + t * ((float)a1 - (float)a0));
    uint32_t r = (uint32_t)(r0 + t * ((float)r1 - (float)r0));
    uint32_t g = (uint32_t)(g0 + t * ((float)g1 - (float)g0));
    uint32_t b = (uint32_t)(b0 + t * ((float)b1 - (float)b0));

    return (a << 24) | (r << 16) | (g << 8) | b;
}

float
peak__edge_cross(float ax, float ay, float bx, float by, float cx, float cy) 
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void
peak_init(void) 
{
    peak_platform_window_open();
    peak_window_buffer = peak_platform_window_buffer(&peak_window_width, &peak_window_height);
    peak_clip_x = 0;
    peak_clip_y = 0;
    peak_clip_width = peak_window_width;
    peak_clip_height = peak_window_height;
}

void
peak_quit(void) 
{
    peak_platform_window_close();
}

void
peak_extensions(void) 
{
    // TODO
}

bool
peak_poll_events(PeakEvent *ev)
{
    return peak_platform_epoll(ev);
}

void
peak_blit(int offset_x, int offset_y, const uint32_t *rgba, size_t width, size_t height) 
{
    for (size_t y = 0; y < height; ++y) {
        int target_y = offset_y + (int)y;
        if (target_y < peak_clip_y || target_y >= (int)(peak_clip_y + peak_clip_height) ||
                target_y < 0 || target_y >= (int)peak_window_height) continue;

        int start_x = offset_x < peak_clip_x ? peak_clip_x : offset_x;
        int end_x = (offset_x + (int)width) > (int)(peak_clip_x + peak_clip_width) 
            ? (int)(peak_clip_x + peak_clip_width) 
            : (offset_x + (int)width);

        if (start_x >= end_x) continue;

        size_t src_offset_x = (size_t)(start_x - offset_x);
        size_t copy_pixels = (size_t)(end_x - start_x);

        memcpy(&peak_window_buffer[target_y * peak_window_width + start_x], &rgba[y * width + src_offset_x], copy_pixels * sizeof(uint32_t));
    }
}

void
peak_clip(int x, int y, size_t w, size_t h) 
{
    peak_clip_x = x;
    peak_clip_y = y;
    peak_clip_width = w;
    peak_clip_height = h;
}

void
peak_clip_reset(void) 
{
    peak_clip_x = 0;
    peak_clip_y = 0;
    peak_clip_width = peak_window_width;
    peak_clip_height = peak_window_height;
}

void
peak_draw_rectangle(int x, int y, size_t w, size_t h, uint32_t color) 
{
    int start_x = (x < peak_clip_x) ? peak_clip_x : x;
    int start_y = (y < peak_clip_y) ? peak_clip_y : y;
    int end_x = (x + (int)w > (int)(peak_clip_x + peak_clip_width)) ? (int)(peak_clip_x + peak_clip_width) : x + (int)w;
    int end_y = (y + (int)h > (int)(peak_clip_y + peak_clip_height)) ? (int)(peak_clip_y + peak_clip_height) : y + (int)h;

    for (int py = start_y; py < end_y; ++py) {
        if (py < 0 || py >= (int)peak_window_height) continue;
        for (int px = start_x; px < end_x; ++px) {
            if (px < 0 || px >= (int)peak_window_width) continue;
            peak_window_buffer[py * peak_window_width + px] = color;
        }
    }
}

void
peak_draw_rectangle_gradient(int x, int y, size_t w, size_t h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3) 
{
    for (size_t ry = 0; ry < h; ++ry) {
        float ty = (h > 1) ? (float)ry / (float)(h - 1) : 0.0f;
        uint32_t left_color  = peak__lerp_color(c0, c3, ty);
        uint32_t right_color = peak__lerp_color(c1, c2, ty);

        for (size_t rx = 0; rx < w; ++rx) {
            float tx = (w > 1) ? (float)rx / (float)(w - 1) : 0.0f;
            uint32_t final_color = peak__lerp_color(left_color, right_color, tx);
            peak__put_pixel(x + (int)rx, y + (int)ry, final_color);
        }
    }
}

void
peak_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) 
{
    int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

    float area = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f) return;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float w0 = peak__edge_cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y);
            float w1 = peak__edge_cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y);
            float w2 = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y);

            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                peak__put_pixel(x, y, color);
            }
        }
    }
}

void
peak_draw_triangle_gradient(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2) 
{
    int min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    int max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    int min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    int max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

    float area = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f) return;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float w0 = peak__edge_cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y) / area;
            float w1 = peak__edge_cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y) / area;
            float w2 = peak__edge_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y) / area;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                uint32_t col_a = peak__lerp_color(c0, c1, w1 / (w0 + w1 + 1e-6f));
                uint32_t final_color = peak__lerp_color(col_a, c2, w2);
                peak__put_pixel(x, y, final_color);
            }
        }
    }
}

void
peak_draw_line(int x0, int y0, int x1, int y1, size_t width, uint32_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int half_w = (int)width / 2;

    while (1) {
        /* Draw square footprint for line thickness */
        for (int ox = -half_w; ox <= half_w; ++ox) {
            for (int oy = -half_w; oy <= half_w; ++oy) {
                peak__put_pixel(x0 + ox, y0 + oy, color);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void
peak_draw_line_gradient(int x0, int y0, int x1, int y1, size_t width, uint32_t c0, uint32_t c1) 
{
    float total_dist = sqrtf((float)((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0)));
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int orig_x0 = x0, orig_y0 = y0;
    int half_w = (int)width / 2;

    while (1) {
        float current_dist = sqrtf((float)((x0 - orig_x0)*(x0 - orig_x0) + (y0 - orig_y0)*(y0 - orig_y0)));
        float t = (total_dist > 0.0f) ? (current_dist / total_dist) : 0.0f;
        uint32_t color = peak__lerp_color(c0, c1, t);

        for (int ox = -half_w; ox <= half_w; ++ox) {
            for (int oy = -half_w; oy <= half_w; ++oy) {
                peak__put_pixel(x0 + ox, y0 + oy, color);
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

