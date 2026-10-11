/* Rend2 public-API demo: explicit discovery, placement, queue completion and heap use. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REND2_IMPLEMENTATION
#include "../../Rend2.h"

#define DEMO_CYCLES 8
#define HEAP_BYTES 4096

static void *caller_storage(size_t size, size_t alignment, void **allocation)
{
	uintptr_t address;
	if (!alignment || (alignment & (alignment - 1)) || size > SIZE_MAX - alignment)
		return NULL;
	*allocation = malloc(size + alignment - 1);
	if (!*allocation)
		return NULL;
	address = (uintptr_t)*allocation;
	address = (address + alignment - 1) & ~(uintptr_t)(alignment - 1);
	return (void *)address;
}

static int report_failure(const char *operation)
{
	RendDiagnostic diagnostic = {0};
	if (rend_last_diagnostic(&diagnostic))
		fprintf(stderr, "rend2 demo: %s: diagnostic %d/native %d: %s\n", operation, diagnostic.code,
		        diagnostic.native_code, diagnostic.message);
	else
		fprintf(stderr, "rend2 demo: %s failed without a diagnostic\n", operation);
	return 0;
}

typedef struct RendDemo {

	RendMemoryRequirements discovery_requirements, backend_requirements;
	RendQueueFamilyInfo families[4096];
	RendDiscovery *discovery;
	RendBackend backend;
	RendQueue queue;
	RendCommandPool pool;
	RendCommandList commands;
	RendTimeline timeline;
	RendHeap heap;
	RendParams params;
	RendQueueParams queue_params;
	RendCommandPoolParams pool_params;
	RendHeapClassInfo classes[64];
	RendHeapDesc heap_desc;
	RendHeapInfo heap_info;
	RendHeapRange range;
	RendSubmitDesc submit;
	void *discovery_allocation, *backend_allocation;
	void *discovery_storage, *backend_storage;
	unsigned char expected[HEAP_BYTES];
	uint32_t class_count, device_index, family, i;
	int success;

} RendDemo;

static int demo_discover(RendDemo *d)
{

	if (!rend_discovery_memory_requirements(&d->discovery_requirements))
		return report_failure("discovery sizing");
	d->discovery_storage =
	    caller_storage(d->discovery_requirements.size, d->discovery_requirements.alignment,
	                   &d->discovery_allocation);
	if (!d->discovery_storage) {
		fprintf(stderr, "rend2 demo: caller discovery storage allocation failed\n");
		return 0;
	}
	if (!rend_discovery_create(d->discovery_storage, d->discovery_requirements.size, d->families,
	                           (uint32_t)(sizeof(d->families) / sizeof(d->families[0])),
	                           &d->discovery)) {
		report_failure("Vulkan device discovery");
		return 0;
	}
	if (!rend_discovery_device_count(d->discovery)) {
		fprintf(stderr, "rend2 demo: unsupported: Vulkan discovery found no physical devices\n");
		return 0;
	}

	for (d->device_index = 0; d->device_index < rend_discovery_device_count(d->discovery);
	     d->device_index++) {
		const RendDeviceInfo *candidate = rend_discovery_device(d->discovery, d->device_index);
		const RendQueueFamilyInfo *candidate_families;
		uint32_t count = 0, j;
		if (!candidate || !rend_device_supports_profile(candidate, REND_PROFILE_GRAPHICS_COMPUTE))
			continue;
		candidate_families = rend_device_queue_families(candidate, &count);
		for (j = 0; j < count; j++) {
			if ((candidate_families[j].capabilities & (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) ==
			    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) {
				d->family = candidate_families[j].family_index;
				break;
			}
		}
		if (d->family != UINT32_MAX) {
			d->params.device = candidate;
			printf("rend2 demo: selected explicit Vulkan device %u: %s\n", d->device_index,
			       rend_device_name(candidate));
			break;
		}
	}
	if (d->family == UINT32_MAX) {
		fprintf(stderr, "rend2 demo: unsupported: no discovered device offers graphics-compute "
		                "profile and queue family\n");
		return 0;
	}

	return 1;
}

static int demo_backend(RendDemo *d)
{
	d->params.profile = REND_PROFILE_GRAPHICS_COMPUTE;
	d->params.queues = &d->queue_params;
	d->params.queue_count = 1;
	d->params.command_pools = &d->pool_params;
	d->params.command_pool_count = 1;
	d->params.heap_count = 1;
	d->params.timeline_count = 1;
	d->queue_params.family_index = d->family;
	d->queue_params.submission_count = 1;
	d->queue_params.max_submit_command_lists = 1;
	d->queue_params.max_submit_waits = 1;
	d->pool_params.family_index = d->family;
	d->pool_params.command_list_count = 1;
	d->pool_params.scratch_bytes = 64 * 1024;
	d->pool_params.metadata_bytes = 16 * 1024;
	if (!rend_memory_requirements(&d->params, &d->backend_requirements)) {
		report_failure("explicit backend sizing");
		return 0;
	}
	d->backend_storage = caller_storage(d->backend_requirements.size,
	                                    d->backend_requirements.alignment, &d->backend_allocation);
	if (!d->backend_storage) {
		fprintf(stderr, "rend2 demo: caller backend storage allocation failed\n");
		return 0;
	}
	if (!rend_place_in_memory(d->backend_storage, d->backend_requirements.size, &d->params,
	                          &d->backend)) {
		report_failure("explicit backend placement");
		return 0;
	}
	if (!rend_queue_get(d->backend, 0, &d->queue) ||
	    !rend_command_pool_get(d->backend, 0, &d->pool)) {
		report_failure("queue/command-pool retrieval");
		return 0;
	}
	if (!rend_timeline_create(d->queue, 0, &d->timeline)) {
		report_failure("timeline creation");
		return 0;
	}

	return 1;
}

static int demo_heap(RendDemo *d)
{
	if (!rend_heap_classes(d->backend, REND_HEAP_DATA, d->classes, 64, &d->class_count) ||
	    !d->class_count) {
		report_failure("data heap memory classes");
		return 0;
	}
	d->heap_desc.kind = REND_HEAP_DATA;
	d->heap_desc.size = HEAP_BYTES;
	for (d->i = 0; d->i < d->class_count; d->i++) {
		if (d->classes[d->i].memory_properties & REND_MEMORY_HOST_VISIBLE) {
			d->heap_desc.memory_class = d->classes[d->i].id;
			break;
		}
	}
	if (d->i == d->class_count) {
		fprintf(stderr, "rend2 demo: unsupported: no host-visible data heap memory class\n");
		return 0;
	}
	if (!rend_heap_create(d->backend, &d->heap_desc, &d->heap) ||
	    !rend_heap_get_info(d->backend, d->heap, &d->heap_info)) {
		report_failure("host-visible data heap creation/info");
		return 0;
	}
	if (!d->heap_info.cpu_base) {
		fprintf(stderr, "rend2 demo: unsupported: selected host-visible heap has no CPU mapping\n");
		return 0;
	}

	return 1;
}

static int demo_submit(RendDemo *d)
{
	for (d->i = 0; d->i < HEAP_BYTES; d->i++)
		d->expected[d->i] = (unsigned char)((d->i * 37u + 11u) & 255u);
	memcpy(d->heap_info.cpu_base, d->expected, sizeof(d->expected));
	d->range.heap = d->heap;
	d->range.offset = 0;
	d->range.size = sizeof(d->expected);
	if (!rend_heap_flush(d->backend, d->range)) {
		report_failure("heap upload flush");
		return 0;
	}
	if (!rend_command_list_begin(d->pool, 0, &d->commands) || !rend_command_list_end(d->commands)) {
		report_failure("command-list recording");
		return 0;
	}
	d->submit.commands = &d->commands;
	d->submit.command_count = 1;
	d->submit.completion.timeline = d->timeline;
	for (d->i = 1; d->i <= DEMO_CYCLES; d->i++) {
		d->submit.completion.value = d->i;
		if (!rend_queue_submit(d->queue, &d->submit)) {
			report_failure("queue submission");
			return 0;
		}
		if (!rend_timeline_wait(d->timeline, d->i, UINT64_MAX)) {
			report_failure("timeline completion wait");
			return 0;
		}
		if (!rend_command_pool_reset(d->pool)) {
			report_failure("command-pool cycle reset");
			return 0;
		}
		if (d->i < DEMO_CYCLES && (!rend_command_list_begin(d->pool, 0, &d->commands) ||
		                           !rend_command_list_end(d->commands))) {
			report_failure("next command-list recording");
			return 0;
		}
	}
	if (!rend_heap_invalidate(d->backend, d->range)) {
		report_failure("heap completion invalidate");
		return 0;
	}
	if (memcmp(d->heap_info.cpu_base, d->expected, sizeof(d->expected))) {
		fprintf(stderr, "rend2 demo: mapped CPU heap data mismatch\n");
		return 0;
	}
	return 1;
}

static int demo_shutdown(RendDemo *d)
{
	/* Terminal failure may leave accepted work pending. Drain before retiring
	 * objects; retain borrowed backing until process exit if retirement fails. */
	if (d->backend) {
		if (!rend_backend_wait_idle(d->backend)) {
			report_failure("backend shutdown drain");
			return 0;
		}
		if (d->pool && !rend_command_pool_reset(d->pool)) {
			report_failure("final command-pool reset");
			return 0;
		}
		if (d->timeline && !rend_timeline_destroy(d->timeline)) {
			report_failure("timeline destruction");
			return 0;
		}
		if (d->heap && !rend_heap_destroy(d->backend, d->heap)) {
			report_failure("heap destruction");
			return 0;
		}
		if (!rend_backend_destroy(d->backend)) {
			report_failure("backend destruction");
			return 0;
		}
	}
	if (d->discovery && !rend_discovery_destroy(d->discovery)) {
		report_failure("discovery destruction");
		return 0;
	}
	free(d->backend_allocation);
	free(d->discovery_allocation);
	return 1;
}

int main(void)
{
	RendDemo demo = {0};
	demo.family = UINT32_MAX;
	int success =
	    demo_discover(&demo) && demo_backend(&demo) && demo_heap(&demo) && demo_submit(&demo);
	if (!demo_shutdown(&demo))
		return 1;
	if (success)
		printf("rend2 demo: %u empty-command queue/timeline cycles and %u-byte mapped CPU "
		       "write/read passed (not GPU data roundtrip)\n",
		       DEMO_CYCLES, HEAP_BYTES);
	return success ? 0 : 1;
}
