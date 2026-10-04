/* Public Rend API stress demo: offscreen texture lifetime, transfers and readback. */
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PEAK_VULKAN
#define PEAK_VULKAN
#endif
#include "../../Peak/peak.h"
#include "../../Rend/rend.h"

static PeakCtx *demo_peak;

#include "../../Peak/peak.c"
#include "../../Rend/rend.c"

#define WIDTH 64
#define HEIGHT 64
#define TARGET_COUNT 2
#define TARGET_PASSES 24
#define FRAMES 8

static int
check(int condition, const char *what)
{
	if (!condition) {
		fprintf(stderr, "rend demo: %s failed\n", what);
		return 0;
	}
	return 1;
}

int
main(void)
{
	RendRenderer renderer = NULL;
	RendTexture targets[TARGET_COUNT] = {0};
	unsigned char pixels[WIDTH * HEIGHT * 4];
	int ok = 0, i, j;

	demo_peak = peak_init_legacy();
	if (!check(demo_peak != NULL, "Peak initialization"))
		goto done;
	renderer = rend_renderer_create_offscreen(WIDTH, HEIGHT, REND_FORMAT_R8G8B8A8_UNORM, REND_BACKEND_VULKAN_14, NULL);
	if (!check(renderer != NULL, "Vulkan offscreen renderer creation (device/driver required)"))
		goto done;
	for (i = 0; i < TARGET_COUNT; i++) {
		uint32_t extent = i ? WIDTH : WIDTH / 2;
		targets[i] = rend_texture_create(renderer, extent, extent, 1, 1, 1, REND_FORMAT_R8G8B8A8_UNORM);
		if (!check(rend_texture_width(&targets[i]) == extent && rend_texture_height(&targets[i]) == extent, "texture target creation"))
			goto done;
	}

	/* Alternate target extents repeatedly within each real asynchronous frame,
	 * forcing pass-local depth replacement through the public render API. */
	for (i = 0; i < FRAMES; i++) {
		if (!check(rend_renderer_frame_begin(renderer), "frame begin"))
			goto done;
		for (j = 0; j < TARGET_PASSES; j++) {
			RendTexture *target = &targets[j % TARGET_COUNT];
			rend_cmd_render_begin_texture(renderer, target);
			rend_cmd_render_end_texture(renderer, target);
		}
		rend_cmd_render_begin(renderer, 1.f, 0.f, 0.f, 1.f);
		rend_cmd_render_end(renderer);
		rend_renderer_frame_end(renderer, NULL);
	}
	rend_renderer_read(renderer, pixels, sizeof(pixels));
	for (i = 0; i < WIDTH * HEIGHT; i++) {
		if (pixels[i * 4] != 255 || pixels[i * 4 + 1] != 0 || pixels[i * 4 + 2] != 0 || pixels[i * 4 + 3] != 255) {
			fprintf(stderr, "rend demo: base clear mismatch at pixel %d\n", i);
			goto done;
		}
	}

	/* Base clear -> texture blit -> preserving passes -> completion/readback. */
	for (i = 0; i < FRAMES; i++) {
		if (!check(rend_renderer_frame_begin(renderer), "transfer frame begin"))
			goto done;
		rend_cmd_render_begin(renderer, 0.f, 1.f, 0.f, 1.f);
		rend_cmd_render_end(renderer);
		rend_cmd_blit(renderer, &targets[0], rend_renderer_color_target(renderer),
			0, 0, WIDTH / 2, HEIGHT / 2, 0, 0, WIDTH / 2, HEIGHT / 2);
		rend_cmd_render_begin_preserve(renderer);
		rend_cmd_render_end(renderer);
		rend_cmd_render_begin_preserve(renderer);
		rend_cmd_render_end(renderer);
		rend_renderer_frame_end(renderer, NULL);
		rend_renderer_read(renderer, pixels, sizeof(pixels));
		for (j = 0; j < WIDTH * HEIGHT; j++) {
			int in_blit = j / WIDTH < HEIGHT / 2 && j % WIDTH < WIDTH / 2;
			unsigned char expected_green = in_blit ? 0 : 255;
			unsigned char expected_alpha = in_blit ? 0 : 255;
			if (pixels[j * 4] != 0 || pixels[j * 4 + 1] != expected_green ||
			    pixels[j * 4 + 2] != 0 || pixels[j * 4 + 3] != expected_alpha) {
				fprintf(stderr, "rend demo: blit/preserve mismatch frame %d pixel %d\n", i, j);
				goto done;
			}
		}
	}
	ok = 1;
	done:
	if (renderer) {
		for (i = 0; i < TARGET_COUNT; i++)
			if (rend_texture_width(&targets[i]))
				rend_texture_destroy(renderer, &targets[i]);
		rend_renderer_destroy(renderer);
	}
	rend_quit();
	peak_quit(demo_peak);
	if (ok)
		puts("rend demo: repeated texture rendering, blit/preserve and readback passed");
	return ok ? 0 : 1;
}
