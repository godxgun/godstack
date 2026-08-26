#ifndef DEMOS_HEADLESS_H
#define DEMOS_HEADLESS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rend.h"

static int
headless_parse(int argc, char **argv, int *frames, const char **ppm)
{
    int i;
    int headless;

    headless = 0;
    *frames = 2;
    *ppm = NULL;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0)
            headless = 1;
        else if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc)
            *ppm = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            *frames = atoi(argv[++i]);
            if (*frames < 1)
                *frames = 1;
        }
    }
    return headless;
}

static int
headless_finish(RendRenderer renderer, uint32_t w, uint32_t h, RendFormat fmt, const char *ppm)
{
    size_t bpp;
    size_t n;
    uint8_t *px;
    size_t i;
    int any;
    FILE *f;
    uint32_t x, y;

    bpp = rend_format_size[fmt];
    n = (size_t)w * (size_t)h * bpp;
    if (!renderer || !bpp || !n)
        return 0;
    px = malloc(n);
    if (!px)
        return 0;
    rend_renderer_read(renderer, px, n);
    any = 0;
    for (i = 0; i < n; i++) {
        if (px[i]) {
            any = 1;
            break;
        }
    }
    if (ppm) {
        f = fopen(ppm, "wb");
        if (!f) {
            free(px);
            return 0;
        }
        fprintf(f, "P6\n%u %u\n255\n", w, h);
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                uint8_t *p = px + ((size_t)y * w + x) * bpp;
                uint8_t rgb[3];
                if (fmt == REND_FORMAT_B8G8R8A8_UNORM || fmt == REND_FORMAT_B8G8R8A8_SRGB) {
                    rgb[0] = p[2];
                    rgb[1] = p[1];
                    rgb[2] = p[0];
                } else {
                    rgb[0] = p[0];
                    rgb[1] = p[1];
                    rgb[2] = p[2];
                }
                fwrite(rgb, 1, 3, f);
            }
        }
        fclose(f);
    }
    free(px);
    return any;
}

#endif
