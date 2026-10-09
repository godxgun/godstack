/* Rend2 public-API demo: explicit discovery, placement, queue completion and heap use. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REND2_IMPLEMENTATION
#include "../../Rend2.h"

#define DEMO_CYCLES 8
#define HEAP_BYTES 4096

static void *
caller_storage(size_t size, size_t alignment, void **allocation)
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

static int
report_failure(const char *operation)
{
	RendDiagnostic diagnostic = {0};
	if (rend_last_diagnostic(&diagnostic))
		fprintf(stderr, "rend2 demo: %s: diagnostic %d/native %d: %s\n", operation,
			diagnostic.code, diagnostic.native_code, diagnostic.message);
	else
		fprintf(stderr, "rend2 demo: %s failed without a diagnostic\n", operation);
	return 0;
}

int
main(void)
{
	RendMemoryRequirements discovery_requirements, backend_requirements;
	RendQueueFamilyInfo families[4096];
	RendDiscovery *discovery = NULL;
	RendBackend backend = NULL;
	RendQueue queue = NULL;
	RendCommandPool pool = NULL;
	RendCommandList commands = NULL;
	RendTimeline timeline = NULL;
	RendHeap heap = NULL;
	RendParams params = {0};
	RendQueueParams queue_params = {0};
	RendCommandPoolParams pool_params = {0};
	RendHeapClassInfo classes[64];
	RendHeapDesc heap_desc = {0};
	RendHeapInfo heap_info;
	RendHeapRange range;
	RendSubmitDesc submit = {0};
	void *discovery_allocation = NULL, *backend_allocation = NULL;
	void *discovery_storage = NULL, *backend_storage = NULL;
	unsigned char expected[HEAP_BYTES];
	uint32_t class_count = 0, device_index, family = UINT32_MAX, i;
	int success = 0;

	if (!rend_discovery_memory_requirements(&discovery_requirements))
		return report_failure("discovery sizing");
	discovery_storage = caller_storage(discovery_requirements.size, discovery_requirements.alignment, &discovery_allocation);
	if (!discovery_storage) {
		fprintf(stderr, "rend2 demo: caller discovery storage allocation failed\n");
		goto done;
	}
	if (!rend_discovery_create(discovery_storage, discovery_requirements.size, families,
			(uint32_t)(sizeof(families) / sizeof(families[0])), &discovery)) {
		report_failure("Vulkan device discovery");
		goto done;
	}
	if (!rend_discovery_device_count(discovery)) {
		fprintf(stderr, "rend2 demo: unsupported: Vulkan discovery found no physical devices\n");
		goto done;
	}

	for (device_index = 0; device_index < rend_discovery_device_count(discovery); device_index++) {
		const RendDeviceInfo *candidate = rend_discovery_device(discovery, device_index);
		const RendQueueFamilyInfo *candidate_families;
		uint32_t count = 0, j;
		if (!candidate || !rend_device_supports_profile(candidate, REND_PROFILE_GRAPHICS_COMPUTE))
			continue;
		candidate_families = rend_device_queue_families(candidate, &count);
		for (j = 0; j < count; j++) {
			if ((candidate_families[j].capabilities & (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) ==
			    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) {
				family = candidate_families[j].family_index;
				break;
			}
		}
		if (family != UINT32_MAX) {
			params.device = candidate;
			printf("rend2 demo: selected explicit Vulkan device %u: %s\n", device_index, rend_device_name(candidate));
			break;
		}
	}
	if (family == UINT32_MAX) {
		fprintf(stderr, "rend2 demo: unsupported: no discovered device offers graphics-compute profile and queue family\n");
		goto done;
	}

	params.profile = REND_PROFILE_GRAPHICS_COMPUTE;
	params.queues = &queue_params;
	params.queue_count = 1;
	params.command_pools = &pool_params;
	params.command_pool_count = 1;
	params.heap_count = 1;
	params.timeline_count = 1;
	queue_params.family_index = family;
	queue_params.submission_count = 1;
	queue_params.max_submit_command_lists = 1;
	queue_params.max_submit_waits = 1;
	pool_params.family_index = family;
	pool_params.command_list_count = 1;
	pool_params.scratch_bytes = 64 * 1024;
	pool_params.metadata_bytes = 16 * 1024;
	if (!rend_memory_requirements(&params, &backend_requirements)) {
		report_failure("explicit backend sizing");
		goto done;
	}
	backend_storage = caller_storage(backend_requirements.size, backend_requirements.alignment, &backend_allocation);
	if (!backend_storage) {
		fprintf(stderr, "rend2 demo: caller backend storage allocation failed\n");
		goto done;
	}
	if (!rend_place_in_memory(backend_storage, backend_requirements.size, &params, &backend)) {
		report_failure("explicit backend placement");
		goto done;
	}
	if (!rend_queue_get(backend, 0, &queue) || !rend_command_pool_get(backend, 0, &pool)) {
		report_failure("queue/command-pool retrieval");
		goto done;
	}
	if (!rend_timeline_create(queue, 0, &timeline)) {
		report_failure("timeline creation");
		goto done;
	}

	if (!rend_heap_classes(backend, REND_HEAP_DATA, classes, 64, &class_count) || !class_count) {
		report_failure("data heap memory classes");
		goto done;
	}
	heap_desc.kind = REND_HEAP_DATA;
	heap_desc.size = HEAP_BYTES;
	for (i = 0; i < class_count; i++) {
		if (classes[i].memory_properties & REND_MEMORY_HOST_VISIBLE) {
			heap_desc.memory_class = classes[i].id;
			break;
		}
	}
	if (i == class_count) {
		fprintf(stderr, "rend2 demo: unsupported: no host-visible data heap memory class\n");
		goto done;
	}
	if (!rend_heap_create(backend, &heap_desc, &heap) || !rend_heap_get_info(backend, heap, &heap_info)) {
		report_failure("host-visible data heap creation/info");
		goto done;
	}
	if (!heap_info.cpu_base) {
		fprintf(stderr, "rend2 demo: unsupported: selected host-visible heap has no CPU mapping\n");
		goto done;
	}

	for (i = 0; i < HEAP_BYTES; i++)
		expected[i] = (unsigned char)((i * 37u + 11u) & 255u);
	memcpy(heap_info.cpu_base, expected, sizeof(expected));
	range.heap = heap;
	range.offset = 0;
	range.size = sizeof(expected);
	if (!rend_heap_flush(backend, range)) {
		report_failure("heap upload flush");
		goto done;
	}
	if (!rend_command_list_begin(pool, 0, &commands) || !rend_command_list_end(commands)) {
		report_failure("command-list recording");
		goto done;
	}
	submit.commands = &commands;
	submit.command_count = 1;
	submit.completion.timeline = timeline;
	for (i = 1; i <= DEMO_CYCLES; i++) {
		submit.completion.value = i;
		if (!rend_queue_submit(queue, &submit)) {
			report_failure("queue submission");
			goto done;
		}
		if (!rend_timeline_wait(timeline, i, UINT64_MAX)) {
			report_failure("timeline completion wait");
			goto done;
		}
		if (!rend_command_pool_reset(pool)) {
			report_failure("command-pool cycle reset");
			goto done;
		}
		if (i < DEMO_CYCLES && (!rend_command_list_begin(pool, 0, &commands) || !rend_command_list_end(commands))) {
			report_failure("next command-list recording");
			goto done;
		}
	}
	if (!rend_heap_invalidate(backend, range)) {
		report_failure("heap completion invalidate");
		goto done;
	}
	if (memcmp(heap_info.cpu_base, expected, sizeof(expected))) {
		fprintf(stderr, "rend2 demo: mapped CPU heap data mismatch\n");
		goto done;
	}
	success = 1;
done:
	/* Terminal failure may leave accepted work pending. Drain before retiring
	 * objects; retain borrowed backing until process exit if retirement fails. */
	if (backend) {
		if (!rend_backend_wait_idle(backend)) {
			report_failure("backend shutdown drain");
			return 1;
		}
		if (pool && !rend_command_pool_reset(pool)) {
			report_failure("final command-pool reset");
			return 1;
		}
		if (timeline && !rend_timeline_destroy(timeline)) {
			report_failure("timeline destruction");
			return 1;
		}
		if (heap && !rend_heap_destroy(backend, heap)) {
			report_failure("heap destruction");
			return 1;
		}
		if (!rend_backend_destroy(backend)) {
			report_failure("backend destruction");
			return 1;
		}
	}
	if (discovery && !rend_discovery_destroy(discovery)) {
		report_failure("discovery destruction");
		return 1;
	}
	free(backend_allocation);
	free(discovery_allocation);
	if (success)
		printf("rend2 demo: %u empty-command queue/timeline cycles and %u-byte mapped CPU write/read passed (not GPU data roundtrip)\n", DEMO_CYCLES, HEAP_BYTES);
	return success ? 0 : 1;
}
