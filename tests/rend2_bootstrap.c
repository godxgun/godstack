/* Rend2 bounded bootstrap/native-state checks.
 * Run from godstack with ./build rend2 test. Vulkan device checks are skipped
 * explicitly when discovery succeeds but finds no physical devices.
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Rend2/rend.h"

/* Intercept requests from the Rend unity module only, not caller backing or
 * dynamically linked Vulkan/driver allocations. Any such request is a regression. */
static uint64_t rend2_host_backing_attempts;
#define malloc(size) ((void)(size), ++rend2_host_backing_attempts, (void *)0)
#define calloc(count, size) ((void)(count), (void)(size), ++rend2_host_backing_attempts, (void *)0)
#define realloc(pointer, size) ((void)(pointer), (void)(size), ++rend2_host_backing_attempts, (void *)0)
#define aligned_alloc(alignment, size) ((void)(alignment), (void)(size), ++rend2_host_backing_attempts, (void *)0)
#define posix_memalign(output, alignment, size) ((void)(output), (void)(alignment), (void)(size), ++rend2_host_backing_attempts, ENOMEM)
#include "../Rend2/rend.c"
#undef malloc
#undef calloc
#undef realloc
#undef aligned_alloc
#undef posix_memalign

static void
check_context_rejection(void)
{
	RendParams params = {0};
	RendMemoryRequirements requirements;
	RendDiagnostic diagnostic;
	RendBackend backend = (RendBackend)(uintptr_t)1;

	requirements.size = SIZE_MAX;
	requirements.alignment = SIZE_MAX;
	assert(!rend_memory_requirements(&params, &requirements));
	assert(requirements.size == 0);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_INVALID_ARGUMENT);

	assert(!rend_memory_requirements(NULL, &requirements));
	assert(requirements.size == 0);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_INVALID_ARGUMENT);
	assert(!rend_memory_requirements(&params, NULL));

	/* No implicit backend/device selection: profiles without an explicit device fail. */
	params.profile = REND_PROFILE_FULL;
	assert(!rend_memory_requirements(&params, &requirements));
	assert(requirements.size == 0);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_INVALID_ARGUMENT);
	assert(!rend_place_in_memory(NULL, 0, &params, &backend));
	assert(backend == NULL);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_INVALID_ARGUMENT);

	backend = (RendBackend)(uintptr_t)1;
	assert(!rend_place_in_memory(NULL, 0, &params, &backend));
	assert(backend == NULL);
	assert(!rend_place_in_memory(NULL, 0, NULL, &backend));
	assert(backend == NULL);
}

static void *
caller_storage(size_t size, size_t alignment, void **out_allocation)
{
	uintptr_t address;
	void *allocation;

	assert(alignment != 0);
	assert(size <= SIZE_MAX - alignment);
	allocation = malloc(size + alignment - 1);
	assert(allocation);
	address = (uintptr_t)allocation;
	address = (address + alignment - 1) & ~(uintptr_t)(alignment - 1);
	*out_allocation = allocation;
	return (void *)address;
}

static void
check_reduced_context(const RendDeviceInfo *device, const RendQueueFamilyInfo *families, uint32_t family_count)
{
	RendParams params = {0};
	RendQueueParams queue_params = {0};
	RendCommandPoolParams pool_params = {0};
	RendMemoryRequirements requirements;
	RendStorageRequirements storage_requirements;
	RendHeapClassInfo classes[64];
	RendHeapClassInfo short_classes[1] = {{UINT32_MAX, UINT32_MAX}};
	RendHeapDesc heap_desc = {0};
	RendBackend backend = NULL;
	RendHeap heap = NULL;
	RendHeap failed_heap = (RendHeap)(uintptr_t)1;
	RendHeap exhausted_heap = (RendHeap)(uintptr_t)1;
	RendHeapInfo heap_info;
	RendHeapRange range;
	RendQueue queue = NULL;
	RendCommandPool pool = NULL;
	RendCommandList commands = NULL;
	RendCommandList failed_commands = (RendCommandList)(uintptr_t)1;
	RendTimeline timeline = NULL;
	RendSubmitDesc submit = {0};
	RendDiagnostic diagnostic;
	void *allocation, *memory;
	uint32_t class_count = 0, required_class_count, family = UINT32_MAX, i;

	for (i = 0; i < family_count; i++) {
		if ((families[i].capabilities & (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) ==
		    (REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE)) {
			family = families[i].family_index;
			break;
		}
	}
	assert(family != UINT32_MAX);

	/* An explicit device/profile request is required; malformed requests publish no handle. */
	params.profile = REND_PROFILE_GRAPHICS_COMPUTE;
	params.device = device;
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
	{
		RendProfile reduced = params.profile;
		params.profile = REND_PROFILE_FULL;
		assert(!rend_memory_requirements(&params, &requirements));
		assert(requirements.size == 0);
		assert(rend_last_diagnostic(&diagnostic));
		assert(diagnostic.code == REND_DIAG_UNSUPPORTED);
		params.profile = reduced;
	}
	queue_params.submission_count = 0;
	assert(!rend_memory_requirements(&params, &requirements));
	assert(requirements.size == 0);
	queue_params.submission_count = 1;
	pool_params.command_list_count = 0;
	assert(!rend_memory_requirements(&params, &requirements));
	assert(requirements.size == 0);
	pool_params.command_list_count = 1;
	params.scratch_bytes = SIZE_MAX;
	assert(!rend_memory_requirements(&params, &requirements));
	assert(requirements.size == 0);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_OVERFLOW || diagnostic.code == REND_DIAG_INVALID_ARGUMENT);
	params.scratch_bytes = 0;
	if (!rend_memory_requirements(&params, &requirements)) {
		assert(requirements.size == 0);
		assert(rend_last_diagnostic(&diagnostic));
		assert(diagnostic.code != REND_DIAG_UNSUPPORTED);
		assert(!"valid explicit reduced context sizing failed");
	}
	assert(requirements.size > 0 && requirements.alignment > 0);
	assert(!(requirements.alignment & (requirements.alignment - 1)));
	memory = caller_storage(requirements.size, requirements.alignment, &allocation);
	assert(!rend_place_in_memory(memory, requirements.size - 1, &params, &backend));
	assert(backend == NULL);
	assert(!rend_place_in_memory((unsigned char *)memory + 1, requirements.size, &params, &backend));
	assert(backend == NULL);
	assert(rend_place_in_memory(memory, requirements.size, &params, &backend));
	assert(backend);
	assert(!rend_heap_create(backend, &heap_desc, &failed_heap));
	assert(failed_heap == NULL);
	assert(rend_queue_get(backend, 0, &queue) && queue);
	assert(rend_command_pool_get(backend, 0, &pool) && pool);

	/* Slot transitions and submission preflight: rejected batches keep executable lists. */
	assert(rend_command_list_begin(pool, 0, &commands) && commands);
	assert(!rend_command_list_begin(pool, 0, &failed_commands));
	assert(failed_commands == NULL);
	assert(rend_command_list_end(commands));
	assert(!rend_command_list_end(commands));
	assert(rend_timeline_create(queue, 0, &timeline) && timeline);
	{
		uint64_t initial_value = UINT64_MAX;
		assert(rend_timeline_query(timeline, &initial_value) && initial_value == 0);
	}
	assert(!rend_timeline_wait(timeline, 1, 0));
	submit.commands = &commands;
	submit.command_count = 1;
	submit.completion.timeline = timeline;
	submit.completion.value = 0;
	assert(!rend_queue_submit(queue, &submit));
	failed_commands = (RendCommandList)(uintptr_t)1;
	assert(!rend_command_list_begin(pool, 0, &failed_commands));
	assert(failed_commands == NULL);
	submit.completion.value = 1;
	assert(rend_queue_submit(queue, &submit));
	assert(rend_timeline_wait(timeline, 1, UINT64_MAX));
	{
		uint64_t completed = 0;
		assert(rend_timeline_query(timeline, &completed) && completed >= 1);
	}
	assert(rend_command_pool_reset(pool));
	assert(rend_command_list_begin(pool, 0, &commands));
	assert(rend_command_list_end(commands));
	submit.completion.value = 1;
	assert(!rend_queue_submit(queue, &submit));
	submit.completion.value = 2;
	assert(rend_queue_submit(queue, &submit));
	assert(rend_timeline_wait(timeline, 2, UINT64_MAX));
	assert(rend_command_pool_reset(pool));
	assert(rend_timeline_destroy(timeline));

	/* Heap class query is all-or-nothing, including undersized output arrays. */
	assert(rend_heap_classes(backend, REND_HEAP_DATA, NULL, 0, &class_count));
	assert(class_count > 0 && class_count <= 64);
	required_class_count = class_count;
	assert(!rend_heap_classes(backend, REND_HEAP_DATA, short_classes, 0, &class_count));
	assert(class_count == required_class_count);
	assert(short_classes[0].id == UINT32_MAX && short_classes[0].memory_properties == UINT32_MAX);
	if (class_count > 1) {
		assert(!rend_heap_classes(backend, REND_HEAP_DATA, short_classes, 1, &class_count));
		assert(class_count > 1);
		assert(short_classes[0].id == UINT32_MAX && short_classes[0].memory_properties == UINT32_MAX);
	}
	assert(rend_heap_classes(backend, REND_HEAP_DATA, classes, 64, &class_count));
	assert(class_count > 0 && class_count <= 64);
	heap_desc.kind = REND_HEAP_DATA;
	heap_desc.memory_class = classes[0].id;
	for (i = 0; i < class_count; i++) {
		if (classes[i].memory_properties & REND_MEMORY_HOST_VISIBLE) {
			heap_desc.memory_class = classes[i].id;
			break;
		}
	}
	heap_desc.size = 4096;
	assert(rend_heap_memory_requirements(backend, &heap_desc, &storage_requirements));
	assert(storage_requirements.size >= heap_desc.size && storage_requirements.alignment > 0);
	assert(!(storage_requirements.alignment & (storage_requirements.alignment - 1)));
	assert(rend_heap_create(backend, &heap_desc, &heap));
	assert(heap);
	assert(!rend_heap_create(backend, &heap_desc, &exhausted_heap));
	assert(exhausted_heap == NULL);
	assert(rend_last_diagnostic(&diagnostic));
	assert(diagnostic.code == REND_DIAG_CAPACITY);
	assert(rend_heap_get_info(backend, heap, &heap_info));
	/* Heap mappings/cache maintenance are checked only for reported host-visible mappings. */
	if (heap_info.cpu_base) {
		assert(heap_info.memory_properties & REND_MEMORY_HOST_VISIBLE);
		assert(heap_info.cache_atom_size > 0);
		assert(!(heap_info.cache_atom_size & (heap_info.cache_atom_size - 1)));
		range.heap = heap;
		range.offset = 0;
		range.size = 1;
		assert(rend_heap_flush(backend, range));
		assert(rend_heap_invalidate(backend, range));
		range.offset = heap_info.size;
		assert(!rend_heap_flush(backend, range));
		range.offset = 0;
		range.size = heap_info.size + 1;
		assert(!rend_heap_invalidate(backend, range));
	}
	/* The required 64-bit arithmetic edge fails before a heap is published. */
	heap_desc.size = UINT64_MAX;
	assert(!rend_heap_memory_requirements(backend, &heap_desc, &storage_requirements));
	assert(rend_heap_destroy(backend, heap));
	assert(rend_backend_destroy(backend));
	free(allocation);
}

static void
check_discovery(void)
{
	RendMemoryRequirements requirements;
	RendDiagnostic diagnostic;
	RendDiscovery *discovery = (RendDiscovery *)(uintptr_t)1;
	RendQueueFamilyInfo *families;
	void *allocation, *storage;
	uint32_t i;
	int cycle;

	assert(rend_discovery_memory_requirements(&requirements));
	assert(requirements.size > 0);
	assert(requirements.alignment > 0);
	assert(!(requirements.alignment & (requirements.alignment - 1)));
	assert(!rend_discovery_memory_requirements(NULL));

	/* Invalid bootstrap calls fail without publishing a discovery handle. */
	discovery = (RendDiscovery *)(uintptr_t)1;
	assert(!rend_discovery_create(NULL, 0, NULL, 0, &discovery));
	assert(discovery == NULL);

	/* Documented worst-case discovery capacity: 32 devices × 128 families. */
	families = malloc(4096 * sizeof(*families));
	assert(families);
	storage = caller_storage(requirements.size, requirements.alignment, &allocation);
	discovery = (RendDiscovery *)(uintptr_t)1;
	assert(!rend_discovery_create(storage, requirements.size - 1, families, 4096, &discovery));
	assert(discovery == NULL);
	discovery = (RendDiscovery *)(uintptr_t)1;
	assert(!rend_discovery_create((unsigned char *)storage + 1, requirements.size, families, 4096, &discovery));
	assert(discovery == NULL);
	free(allocation);

	for (cycle = 0; cycle < 2; cycle++) {
		uint32_t total_families = 0;

		storage = caller_storage(requirements.size, requirements.alignment, &allocation);
		discovery = (RendDiscovery *)(uintptr_t)1;
		assert(rend_discovery_create(storage, requirements.size, families, 4096, &discovery));
		assert(discovery);
		assert(!rend_discovery_device(discovery, rend_discovery_device_count(discovery)));

		if (!rend_discovery_device_count(discovery)) {
			puts("rend2_bootstrap: SKIP native device/capability checks (no Vulkan devices found)");
		} else {
			puts("rend2_bootstrap: checking discovered Vulkan device information");
			for (i = 0; i < rend_discovery_device_count(discovery); i++) {
				const RendDeviceInfo *device = rend_discovery_device(discovery, i);
				const RendQueueFamilyInfo *device_families;
				const char *name;
				uint32_t family_count = 0, j;
				RendParams params = {0};
				RendMemoryRequirements context_requirements;
				int supported;

				assert(device);
				assert(rend_device_backend(device) == REND_BACKEND_VULKAN);
				name = rend_device_name(device);
				assert(name && name[0]);
				device_families = rend_device_queue_families(device, &family_count);
				assert(device_families || family_count == 0);
				assert(family_count <= 4096 - total_families);
				total_families += family_count;
				for (j = 0; j < family_count; j++) {
					assert(device_families[j].queue_count > 0);
					assert(!(device_families[j].capabilities & ~(REND_QUEUE_GRAPHICS | REND_QUEUE_COMPUTE | REND_QUEUE_TRANSFER)));
				}
				/* Full requires mesh work; never fall back to the reduced profile. */
				params.profile = REND_PROFILE_FULL;
				params.device = device;
				assert(!rend_memory_requirements(&params, &context_requirements));
				assert(context_requirements.size == 0);
				assert(rend_last_diagnostic(&diagnostic));
				assert(diagnostic.code == REND_DIAG_UNSUPPORTED || diagnostic.code == REND_DIAG_INVALID_ARGUMENT);
				assert(rend_device_supports_profile(device, REND_PROFILE_FULL) == 0);

				supported = rend_device_supports_profile(device, REND_PROFILE_GRAPHICS_COMPUTE);
				assert(supported == 0 || supported == 1);
				printf("rend2_bootstrap: device %s, %u queue families; graphics-compute %s\n",
				       name, family_count, supported ? "supported" : "unsupported");
				if (supported)
					check_reduced_context(device, device_families, family_count);
			}
		}

		assert(rend_last_diagnostic(&diagnostic));
		assert(rend_discovery_destroy(discovery));
		assert(!rend_discovery_destroy(discovery));
		assert(rend_discovery_device_count(discovery) == 0);
		assert(!rend_discovery_device(discovery, 0));
		/* Failed calls do not retain the reusable discovery backing. */
		discovery = (RendDiscovery *)(uintptr_t)1;
		assert(!rend_discovery_create(storage, requirements.size, NULL, 1, &discovery));
		assert(discovery == NULL);
		assert(!rend_discovery_create(storage, requirements.size, families, 4096, NULL));
		if (total_families) {
			discovery = (RendDiscovery *)(uintptr_t)1;
			assert(!rend_discovery_create(storage, requirements.size, families, total_families - 1, &discovery));
			assert(discovery == NULL);
			assert(rend_last_diagnostic(&diagnostic));
			assert(diagnostic.code == REND_DIAG_CAPACITY);
			assert(rend_discovery_create(storage, requirements.size, families, total_families, &discovery));
			assert(rend_discovery_destroy(discovery));
		}
		free(allocation);
	}
	free(families);
}

int
main(void)
{
	puts("rend2_bootstrap: checking zero-initialized and explicit-device context configuration");
	check_context_rejection();
	puts("rend2_bootstrap: checking caller-backed discovery sizing and repeated teardown");
	check_discovery();
	assert(rend2_host_backing_attempts == 0);
	puts("rend2_bootstrap: Rend-controlled host heap backing attempts 0; caller/Vulkan/driver allocations excluded");
	puts("rend2_bootstrap: passed discovery and any explicitly supported reduced-profile native checks");
	return 0;
}
