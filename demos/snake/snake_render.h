/* Snake demo Rend2 resources and explicit one-in-flight rendering policy. */
#ifndef DEMOS_SNAKE_RENDER_H
#define DEMOS_SNAKE_RENDER_H

#include <stdint.h>

#include <vulkan/vulkan.h>

#include "../../Rend2.h"
#include "Peak.h"
#include "snake_gpu.h"

typedef struct SnakeRender {
	void *discovery_memory;
	void *backend_memory;
	RendQueueFamilyInfo family_storage[4096];
	RendDiscovery *discovery;
	RendBackend backend;
	RendQueue queue;
	RendCommandPool pool;
	RendTimeline timeline;
	RendHeap data_heap;
	RendHeap texture_heap;
	RendHeap depth_heap;
	RendTexture depth_texture;
	RendTextureView depth_view;
	RendTexture offscreen_texture;
	RendTextureView offscreen_view;
	RendPipeline pipeline;
	RendPresentation presentation;
	VkSurfaceKHR surface;
	PeakWindow *window;
	const uint8_t *vertex_spv;
	const uint8_t *fragment_spv;
	size_t vertex_bytes, fragment_bytes;
	RendHeapInfo data_info;
	uint64_t vertex_offset, index_offset, instance_offset, root_offset, readback_offset;
	uint64_t timeline_value;
	uint32_t width, height;
	RendFormat format;
	RendDiagnostic failure;
	int headless;
	int submitted;
} SnakeRender;

static int snake_render_init(SnakeRender *r, int headless, uint32_t width, uint32_t height,
                             uint32_t device_index, PeakWindow *window, const uint8_t *vertex_spv,
                             size_t vertex_bytes, const uint8_t *fragment_spv, size_t fragment_bytes);
static int snake_render_draw(SnakeRender *r, const SnakeGpuVertex *vertices,
                             const uint16_t *indices, uint32_t index_count, const SnakeGpuInstance *instances,
                             uint32_t instance_count, const SnakeGpuRoot *root, int readback);
static int snake_render_drain(SnakeRender *r);
static int snake_render_minimize(SnakeRender *r);
static int snake_render_recreate_presentation(SnakeRender *r, uint32_t width, uint32_t height);
static int snake_render_capture(SnakeRender *r, const char *ppm);
static int snake_render_destroy(SnakeRender *r);

#endif
