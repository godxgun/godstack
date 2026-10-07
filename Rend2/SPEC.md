# Rend 2 Specification

## Scope and purpose

Rend sends data to the GPU through a small, low-level graphics/compute API: allocate heaps,
reference application data from shaders, record work, and submit it with explicit synchronization.
It removes native boilerplate without taking over application policy.

Version 2.0.0 specifies a general-purpose graphics/compute core with explicit contracts and practical performance.

Conformance does not require every native GPU feature, universal optimality, or speculative extensibility. Metal 4
and DirectX 12 remain full-target requirements; Vulkan conformance does not require their availability.

## Design principles

- **No application-data ownership.** Rend manages its bookkeeping and the native objects it creates until explicit destruction; it does not own application payloads or allocation/retirement policy.
- **Zero is initialization (ZII).** Zero-initialized descriptions and handles are safe stub/empty states, not implicit device selection, resource creation, or allocation. Operations requiring configured resources fail when those resources are absent. Success is nonzero; failure is zero/null.
- **No dynamic reallocation.** Exhaustion is an explicit failure, not permission to grow or evict live storage.
- **Minimal hot-path work. Respect and reference the graphics API specification.** Validate immutable configuration at preparation boundaries; retain required capacity and error checks rather than pursuing branchlessness at the expense of correctness.
- Data is memory, not a zoo of vertex, constant, storage, and transfer buffer objects.
- The application owns allocation policy, data layouts, descriptor indices, resource lifetimes, and GPU synchronization. Rend reports native requirements and provides the operations needed to honor them.
- Use GPU addresses for ordinary data and **bindless** indices for textures/samplers. Each draw or dispatch takes a root argument address.
- Keep backend-specific binding machinery out of the application interface. Do not pretend it disappears from native APIs where it remains necessary.
- Prefer fixed capacities, explicit ownership, direct operations, and clear failure behavior. Do not introduce a render graph, material system, general object framework, or implicit per-frame resource manager.
- No allocation churn in Rend-controlled host storage. Measure actual backing, latency, and allocation behavior; distinguish capacities from measurements and native allocations from Rend allocations.
- Unsupported hardware or shader targets must fail with a useful reason. Do not silently fall back to the old descriptor-set/vertex-layout programming model.

# Specification and API Design

## Basic Structures

**Foundational API draft, not a complete or frozen public header.** These are host-side declarations. Ordinary shader data uses 64-bit GPU addresses, not host handles or per-allocation buffer IDs. Texture/sampler shader indices are `uint32_t` slots, distinct from host texture/view handles.

```c
#include <stddef.h>
#include <stdint.h>

typedef enum RendProfile {
	REND_PROFILE_NONE = 0,              /* Unconfigured stub; cannot create a usable context. */
	REND_PROFILE_FULL,                  /* Includes required task/mesh support. */
	REND_PROFILE_GRAPHICS_COMPUTE       /* Explicit reduced profile; no fallback. */
} RendProfile;

/* Opaque host records; never serialize these pointers into shader data. */
typedef struct RendDeviceInfo RendDeviceInfo;
typedef struct RendTextureDesc RendTextureDesc;
typedef struct RendBackendState *RendBackend;
typedef struct RendHeapState *RendHeap;
typedef struct RendTextureState *RendTexture;
typedef struct RendTextureViewState *RendTextureView;
typedef struct RendPipelineState *RendPipeline;
typedef struct RendQueueState *RendQueue;
typedef struct RendCommandPoolState *RendCommandPool;
typedef struct RendCommandListState *RendCommandList;
typedef struct RendTimelineState *RendTimeline;
typedef struct RendPresentationState *RendPresentation;

typedef struct RendQueueParams {
	uint32_t family_index;             /* Backend-local family from device discovery. */
	uint32_t queue_index;              /* Native queue within that family; no duplicate selections. */
	uint32_t submission_count;         /* Maximum outstanding submissions on this queue. */
	uint32_t max_submit_command_lists;
	uint32_t max_submit_waits;
} RendQueueParams;

typedef struct RendCommandPoolParams {
	uint32_t family_index;             /* Must be among the enabled queue families. */
	uint32_t command_list_count;       /* Fixed slots, reused by resetting the whole pool. */
	size_t scratch_bytes;              /* Private recording workspace; never shared with another pool. */
	size_t metadata_bytes;             /* Bounded native-lowering metadata, if needed; not a replay stream. */
} RendCommandPoolParams;

typedef struct RendParams {
	RendProfile profile;
	const RendDeviceInfo *device;      /* Explicitly selected, backend-matching discovery record. */
	const RendQueueParams *queues;
	const RendCommandPoolParams *command_pools;

	/* Context-wide resource maxima; zero disables creation of that category. */
	uint32_t heap_count;
	uint32_t texture_count;
	uint32_t texture_view_count;
	uint32_t sampler_count;
	uint32_t pipeline_count;
	uint32_t timeline_count;
	uint32_t presentation_count;

	/* Arrays are read during sizing/placement and copied into placed backing. */
	uint32_t queue_count;
	uint32_t command_pool_count;
	size_t scratch_bytes;
} RendParams;

typedef struct RendMemoryRequirements {
	size_t size;
	size_t alignment;
} RendMemoryRequirements;

/* Status returns: nonzero success, zero failure. Outputs carry queried values/handles. */
/* Host sizing creates no GPU heaps, textures, or descriptor entries. */
int rend_memory_requirements(const RendParams *params, RendMemoryRequirements *out_requirements);
int rend_place_in_memory(void *memory, size_t memory_size, const RendParams *params, RendBackend *out_backend);
int rend_backend_destroy(RendBackend backend);
/* Explicit blocking drain, including failed-submit cleanup; never called implicitly. */
int rend_backend_wait_idle(RendBackend backend);
int rend_queue_get(RendBackend backend, uint32_t index, RendQueue *out_queue);
int rend_command_pool_get(RendBackend backend, uint32_t index, RendCommandPool *out_pool);

typedef enum RendHeapKind {
	REND_HEAP_NONE = 0,                /* Unconfigured stub; no backing is requested. */
	REND_HEAP_DATA,
	REND_HEAP_TEXTURE_STORAGE,
	REND_HEAP_TEXTURE_DESCRIPTORS,
	REND_HEAP_SAMPLER_DESCRIPTORS
} RendHeapKind;

enum RendMemoryProperty {
	REND_MEMORY_DEVICE_LOCAL = 1u << 0,
	REND_MEMORY_HOST_VISIBLE = 1u << 1,
	REND_MEMORY_HOST_COHERENT = 1u << 2,
	REND_MEMORY_HOST_CACHED = 1u << 3
};

typedef struct RendHeapClassInfo {
	uint32_t id;                       /* Nonzero, context-local, specific to the queried heap kind. */
	uint32_t memory_properties;        /* Actual properties; caller selects an acceptable class. */
} RendHeapClassInfo;

typedef struct RendHeapDesc {
	RendHeapKind kind;
	uint32_t memory_class;             /* Explicit class ID; zero does not mean automatic selection. */
	uint64_t size;                     /* Requested bytes for data/texture storage. */
	uint32_t descriptor_count;         /* Requested slots for descriptor heaps; otherwise zero. */
} RendHeapDesc;

typedef struct RendStorageRequirements {
	uint64_t size;                     /* Native backing/placement bytes, including required padding. */
	uint64_t alignment;
} RendStorageRequirements;

typedef struct RendHeapInfo {
	RendHeapKind kind;
	uint32_t memory_class;
	uint64_t size;                     /* Usable byte extent, not a texture's linear pixel size. */
	void *cpu_base;                    /* NULL unless mapped. */
	uint64_t gpu_base;                 /* Shader-data address for data heaps; zero otherwise. */
	uint64_t cache_atom_size;
	uint32_t memory_properties;
	uint32_t descriptor_count;         /* Usable slots; zero for data/texture storage. */
} RendHeapInfo;

typedef struct RendHeapRange {
	RendHeap heap;
	uint64_t offset;
	uint64_t size;
} RendHeapRange;

/* Count query: classes = NULL and capacity = 0. No partial array publication. */
int rend_heap_classes(RendBackend backend, RendHeapKind kind, RendHeapClassInfo *classes, uint32_t capacity, uint32_t *out_count);
/* Query compatibility with a selected class BEFORE allocating its backing. */
int rend_texture_memory_requirements(RendBackend backend, const RendTextureDesc *desc, uint32_t memory_class, RendStorageRequirements *out_requirements);
/* Application suballocation needs no Rend record or allocation call per range. */
int rend_heap_memory_requirements(RendBackend backend, const RendHeapDesc *desc, RendStorageRequirements *out_requirements);
int rend_heap_create(RendBackend backend, const RendHeapDesc *desc, RendHeap *out_heap);
int rend_heap_get_info(RendBackend backend, RendHeap heap, RendHeapInfo *out_info);
int rend_heap_flush(RendBackend backend, RendHeapRange range);
int rend_heap_invalidate(RendBackend backend, RendHeapRange range);
int rend_heap_destroy(RendBackend backend, RendHeap heap);

enum RendStage {
	REND_STAGE_NONE = 0,
	REND_STAGE_HOST = 1u << 0,
	REND_STAGE_TRANSFER = 1u << 1,
	REND_STAGE_COMPUTE = 1u << 2,
	REND_STAGE_INDEX_INPUT = 1u << 3,
	REND_STAGE_VERTEX = 1u << 4,
	REND_STAGE_TASK = 1u << 5,
	REND_STAGE_MESH = 1u << 6,
	REND_STAGE_FRAGMENT = 1u << 7,
	REND_STAGE_EARLY_DEPTH_STENCIL = 1u << 8,
	REND_STAGE_LATE_DEPTH_STENCIL = 1u << 9,
	REND_STAGE_COLOR_OUTPUT = 1u << 10,
	REND_STAGE_INDIRECT = 1u << 11,
	REND_STAGE_GRAPHICS = 1u << 12      /* Aggregate: all enabled graphics stages, not compute. */
};

enum RendAccess {
	REND_ACCESS_NONE = 0,              /* Execution dependency only when both access masks are zero. */
	REND_ACCESS_READ = 1u << 0,
	REND_ACCESS_WRITE = 1u << 1
};

typedef struct RendBarrier {
	uint32_t producer_stages;
	uint32_t producer_access;
	uint32_t consumer_stages;
	uint32_t consumer_access;
} RendBarrier;

typedef struct RendTimelinePoint {
	RendTimeline timeline;
	uint64_t value;
} RendTimelinePoint;

typedef struct RendTimelineWait {
	RendTimelinePoint point;
	uint32_t consumer_stages;          /* Earliest consumers; HOST/NONE are not queue wait scopes. */
} RendTimelineWait;

typedef struct RendSubmitDesc {
	const RendCommandList *commands;
	uint32_t command_count;
	const RendTimelineWait *waits;
	uint32_t wait_count;
	RendTimelinePoint completion;      /* Required signal covering ALL work in this submission. */
} RendSubmitDesc;

/* Queues/pools are borrowed from placement; reset never waits for pending work. */
int rend_command_list_begin(RendCommandPool pool, uint32_t slot, RendCommandList *out_commands);
int rend_command_list_end(RendCommandList commands);
int rend_command_pool_reset(RendCommandPool pool);
/* Visibility/order only: no resource list, CPU wait, or automatic hazard detection. */
int rend_cmd_barrier(RendCommandList commands, const RendBarrier *barrier);
int rend_queue_submit(RendQueue queue, const RendSubmitDesc *desc);
/* Each timeline has exactly one producing queue; any queue in the context may wait. */
int rend_timeline_create(RendQueue producer, uint64_t initial_value, RendTimeline *out_timeline);
int rend_timeline_query(RendTimeline timeline, uint64_t *out_value);
int rend_timeline_wait(RendTimeline timeline, uint64_t value, uint64_t timeout_ns);
int rend_timeline_destroy(RendTimeline timeline);
```

- `RendBackend` is the placed context for the single statically linked backend. Counts reserve bookkeeping, not application textures or shader-data allocations. Placement also creates the configured native queues/command pools/list slots; this native command storage is separate from Rend host backing. Descriptor capacities are per heap. Pool metadata may be zero where direct native recording needs no variable-length Rend metadata.
- `rend_memory_requirements` calculates checked size/alignment from the device and complete configuration, including per-queue submission arrays and private per-pool workspace. Placement requires the same inputs and adequate aligned backing. Pools and queues are indexed by their order in `RendParams`, not by native handle values. Queue count is nonzero; every enabled queue has nonzero submission/command-list limits. Zero pool count allows a context with no recording; every configured pool has nonzero list capacity. Resource counts of zero disable that category, never select defaults.
- Discovery is a separate caller-backed object: query its bytes/alignment, place it into caller storage, and enumerate immutable devices/capabilities before sizing a context. It provides the linked backend/device identity, queue families/counts/capabilities, and discovery/scratch requirements. `RendDeviceInfo` and its owning discovery storage remain immutable and live through teardown of every context using them; discovery teardown precedes release of that backing. Other parameter arrays are copied at context placement and may then be released. Concrete bootstrap entry points and operation-specific descriptions, including `RendTextureDesc`, remain deferred; these foundational declarations are not a complete header.
- Data/texture-storage heaps require nonzero `RendHeapDesc.size` and zero `descriptor_count`; descriptor heaps require zero `size` and nonzero `descriptor_count`. Select a reported class explicitly; no automatic memory-type fallback exists. Class IDs are valid only for their context and heap kind. Texture requirements validate a selected texture-storage class before backing is allocated; size/alignment alone do not establish compatibility. A class query with `NULL, 0` returns the required count; an undersized non-query array returns zero, reports the required count, and writes no entries.
- Recording ownership, state transitions, and submission behavior are defined in [REND-WORK-001](#rend-work-001--graphics-compute-and-mesh-work); completion/retirement in [REND-SYNC-002](#rend-sync-002--explicit-completion-visibility-and-safe-reuse). Independent pools and queues do not share mutable hot-path scratch. There is no context-wide recording lock.

The package version 2.0.0 alone does not establish API compatibility. The linked implementation must match the public header; shader-data ABI identities and compatibility checks remain required. There is no runtime-module ABI.

### Common API contract

- **Inputs/ZII:** start descriptions in their zero-initialized stub/empty state and configure the fields required by the operation. Zero capacities reserve no storage; zero handles/addresses mean absent; zero configuration must not select a device, request backing, or silently choose a usable profile/heap kind. Zero-valued indices, offsets, flags, and counters retain their documented meanings; ZII does not make descriptor index zero a null descriptor. Unknown enum values/flag bits, nonzero unused fields, incompatible kinds, and invalid state combinations fail. An array pointer may be `NULL` only with count zero; nonzero counts require adequate caller storage. Output pointers are required unless explicitly optional. Unless explicitly borrowed above, input descriptions/arrays are consumed during the call, not retained by address. Referenced handles/backing retain their longer documented lifetimes. Writable outputs/diagnostics must not overlap input or live Rend storage.
- **Bounds:** check multiplication, addition, alignment rounding, and conversion to native sizes before mutation. Heap ranges require `offset <= extent` and `size <= extent - offset`; operations add their own nonzero-size, stride, alignment, and kind constraints. Report power-of-two alignments. Never truncate 64-bit extents into native narrower fields. Raw GPU addresses are different: Rend cannot prove their allocation bounds or follow arbitrary pointer graphs without the forbidden per-allocation tracking.
- **Handles:** handles are borrowed references to live context records, not ownership-bearing values. Queue/pool handles live until context teardown; list handles are invalidated by pool reset. Other handles remain live until successful explicit destruction. Handles must belong to the same context and be live when passed. Foreign/dangling pointers, shader out-of-bounds access, and use-after-free are caller errors, not a promise of recoverable runtime validation. Validate known state/capacity; do not scan all resources per command to simulate memory safety.
- **Results/diagnostics:** status-returning operations use `int`: nonzero means success, zero means failure; no negative error-code convention is used. Handle-returning operations, when specified, use a nonzero handle for success and null for failure. Status is separate from queried data, so a successful query may report a zero count or timeline value. Creation and handle-get/begin functions clear a valid output-handle slot on failure. Other outputs are valid only on success, except documented count-query results. Failure reasons still include malformed requests, invalid transitions, outstanding use, capacity exhaustion, unsupported capabilities/placement, native failures, timeout, out-of-date presentation, and device loss; they are not public result codes. The separate diagnostic/state-reporting mechanism is deferred and must make the distinctions needed for recovery observable without changing the zero/null failure convention. No result enum or per-call error record is required; the API remains incomplete until those recovery observations are specified.
- **Failure/teardown:** failed creation publishes no object, releases native objects acquired by the attempt, and leaves no references into failed backing. Preflight destruction failure leaves the object live; successful destruction invalidates its handle. The caller retires referenced work and destroys dependent objects before their backing heaps and context. Rend fails for known pending uses but cannot detect arbitrary shader references; safe destruction remains a caller precondition. No destroy/reset performs an implicit wait. Native submission failure and device-loss handling follow [REND-WORK-001](#rend-work-001--graphics-compute-and-mesh-work), not an invented rollback promise.

## Feature summary

| Area                                                                            | Rend 2.0.0 target                                                                        |
| ------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| [Backends](#backends-and-presentation)                                         | Vulkan 1.4, DirectX 12, Metal 4 targets; exactly one statically linked backend                                |
| [Host placement](#host-memory-and-initialization)                               | `RendParams`, `rend_memory_requirements`, `rend_place_in_memory`; bounded caller backing |
| [GPU memory](#rend-mem-003--explicit-gpu-heaps-and-application-allocation)      | Explicit heaps and application suballocation; CPU mappings and 64-bit GPU data addresses |
| [Shader data](#shader-data-and-binding-model)                                   | Typed GPU pointers, shared host/Slang structures, arrays and nested pointer graphs       |
| [Textures](#textures-and-transfers)                                             | Placement requirements, views, uploads, copies, writes, readback                         |
| [Binding](#rend-bind-001--application-owned-bindless-texturesampler-heaps)      | Application-owned texture/sampler slots; per-command root argument addresses             |
| [Work](#commands-and-pipeline-state)                                            | Independent recorders; direct/indirect work; GPU-counted conventional multi-draw         |
| [Pipeline state](#rend-pipe-001--narrow-pipeline-objects-and-independent-state) | No public resource/vertex layouts; independent viewport, scissor, depth/stencil state    |
| [Synchronization](#synchronization-and-lifetime)                                | Global producer/consumer barriers; explicit completion; no application image layouts     |
| [Presentation](#rend-present-001--presentation-and-offscreen-operation)         | Native presentation and offscreen targets; host-owned window policy                      |
| [Utilities](#optional-utilities-and-exclusions)                                 | Optional helpers, independent of the graphics API                                        |
| [Performance](#rend-perf-001--representative-cost-and-performance-acceptance)    | Practical workload budgets; direct-native comparison; no universal-optimality claim     |

### Terms and boundaries

- **Host backing:** caller-owned CPU storage for Rend bookkeeping; not GPU resource storage.
- **Data heap:** GPU-addressable linear storage for vertices, arguments, indices, and arbitrary structures. It may have a CPU mapping.
- **Texture storage:** native backing for placed images. Compatibility is queried; it need not share a native heap type with data storage and does not imply a shader-dereferenceable address or linear pixel mapping.
- **Descriptor heap:** indexed texture-view or sampler entries, not image pixels. Application ownership means choosing capacity and slots; native descriptor representation may remain opaque.
- **Root:** the application-owned argument record reached through the GPU address supplied to a draw/dispatch.

Placement addresses host bookkeeping; it does not bound GPU allocations or driver/OS memory. The allocation boundary is [REND-MEM-002](#rend-mem-002--zero-rend-owned-heap-allocations). Safe reuse is governed by [REND-SYNC-002](#rend-sync-002--explicit-completion-visibility-and-safe-reuse), not by mapping, recording, frame acquisition, or submission alone.

## Host memory and initialization

### REND-MEM-001 — Caller-backed Rend storage

**Required behavior:** `RendParams` specifies Rend bookkeeping capacities/configuration before placement. `rend_memory_requirements` reports bytes and alignment; `rend_place_in_memory` validates caller backing and initializes the context there. The foundational declarations above define the capacity/ownership model; concrete bootstrap entry points and remaining operation descriptions must honor it.

Cover heap/resource/view/sampler/pipeline records, descriptor bookkeeping, queues, command/submission slots, synchronization, presentation, diagnostics, and backend state. Partition recording scratch/metadata per pool and submission storage per queue at placement. Variable-length native-lowering metadata must fit its declared pool byte limit; this is not a requirement to retain a second command stream. Report backend-specific requirements rather than a guessed universal size. Shared management scratch is never borrowed by recording/submission paths.

The query must identify the linked backend, device/capability inputs, and caller-backed discovery/bootstrap/scratch needs, including how those inputs are obtained before placing the context being sized. Requirements are valid only for that configuration; reject unknown inputs or a mismatched placement. Backing remains caller-owned until context teardown; it cannot be relocated or released while Rend uses it.

**Acceptance:** Check exact-size/aligned, undersized/misaligned, zero/maximum capacities, overflow, unsupported inputs, and repeated placement/teardown. Verify zero-initialized unconfigured descriptions fail safely without implicit device selection or native resource creation, failed handle outputs are null, and status is nonzero only on success. Successful count/timeline queries may report zero without being mistaken for failure. Inject partial initialization failure: publish no usable context, release native objects acquired by the attempt, and permit backing reuse. In-bounds initialization writes need not be rolled back; out-of-bounds writes and surviving references into failed backing are forbidden.

### REND-MEM-002 — Zero Rend-owned heap allocations

**Required behavior:** Requirements calculation, placement, operation, failure cleanup, and teardown must request no host heap backing for Rend bookkeeping. Suballocation of caller backing is permitted. Exhaustion returns zero/null failure, never hidden growth, live-object eviction, or fallback allocation. This includes allocator callbacks or helper/utility calls that allocate on Rend's behalf.

Explicit GPU-heap creation requests native backing. Native device/resource/pipeline creation, drivers, compilers, and OS services may allocate internally; report these separately where observable. The guarantee does not cover those external allocations or total process memory. Process-wide zero allocations are outside this guarantee.

**Acceptance:** Instrument Rend-controlled backing calls across placement, resource creation/destruction, recording/submission, synchronization, teardown, and failure workloads. Report allocation attempts/failures, peak live bytes, retained capacity, and exclusions; exhaust each bounded storage category.

### REND-MEM-003 — Explicit GPU heaps and application allocation

**Required behavior:** Create/release heaps with explicit byte sizes and application-selected memory classes; the application suballocates them. Report native size/alignment, granularity, usage, placement, and compatibility constraints. Do not imply that data and texture storage are interchangeable.

Enumerate immutable, backend-local classes for each heap kind with their actual memory properties. The caller selects a class satisfying its required properties; heap creation must use it or fail, never choose a different class. Texture queries must test their description against the selected class before backing is requested. On Vulkan the mapping must honor resource `memoryTypeBits` as well as memory property flags; other backends must honor their native heap/resource restrictions. Class equality does not waive per-description placement, aliasing, or dedicated-storage restrictions. Reject combinations the placement API cannot express rather than creating hidden replacement heaps.

Support device-local data, CPU-write/upload, and CPU-read/readback storage; also mapped device-local data where exposed by the device. Unsupported properties fail explicitly through the absence of a suitable class. Discrete/UMA mappings, coherency, and performance need not match.

Vertex/constant/storage data are heap ranges, not separate public buffer types. Native buffer/resource objects may still be required internally. Textures retain objects/metadata for their opaque layout, format, dimensions, and views; this is not an application allocation policy.

**Acceptance:** Suballocate mixed vertex, argument, and arbitrary-structure data; transfer and read it from shaders without public buffers or per-allocation descriptors. Check bounds, alignment, unsupported properties, and reclamation after completion. Exercise class enumeration/count exhaustion and the texture-description→class validation→heap creation→placement path for sampled, storage, and depth textures. Reject incompatible classes before backing allocation; do not infer compatibility from matching property flags.

### REND-MEM-004 — CPU/GPU addresses and coherency

**Required behavior:** A mapped data heap exposes CPU/GPU bases and byte extent. A checked offset identifies the same allocation through either base; an unmapped heap has no CPU pointer. GPU addresses are 64-bit values scoped to a live device/context allocation, not CPU pointers or portable persistent identifiers.

Mappings and GPU bases remain stable until heap destruction; data address zero is reserved as null. Pointer arithmetic must remain within the live allocation and meet the accessed type/operation alignment. Null can be stored/compared, not dereferenced; a command whose shader reads a root requires a nonzero, ABI-aligned root address. Texture/descriptor storage is not ordinary shader-dereferenceable data, and host handles/pointers must not be substituted for GPU addresses.

Document residency and cache operations. Flush/invalidate validate the logical range, then round to native cache atoms without extending beyond the mapped native allocation; the caller must own/synchronize every touched atom, including padding shared by adjacent suballocations. Coherent mappings may make cache operations no-ops, not completion waits. Coherent CPU writes neither wait for earlier GPU reads nor authorize overwriting their data. GPU output requires completion and any required invalidation before CPU access.

**Acceptance:** Verify base/offset correspondence, unmapped storage, bounds/overflow rejection, CPU-write→shader visibility, and GPU-write→CPU readback after the required synchronization/cache operations.

## Shader data and binding model

### REND-DATA-001 — Typed pointers and a shared data ABI

**Required behavior:** Slang shaders follow typed 64-bit GPU pointers in shared host C++/Slang structures. Support arrays, pointer arithmetic, and nested reads/writes without per-allocation buffer descriptors or binding slots. Vertex shaders fetch through vertex/instance IDs; pipelines have no vertex-layout declarations.

The host API is C99-compatible regardless of backend implementation language. C99 callers use explicit 64-bit GPU-address fields. Shared declarations must verify pointer representation, scalar widths, size, alignment, padding, and matrix conventions for each host/shader target. Native C++ pointers are not automatically a valid serialized GPU representation. Exclude implicit CPU pointers, host ownership objects, virtual members, and compiler-dependent `bool` layouts.

Define GPU-data address spaces, alignment, nulls, mutability, and legal casts. This does not require arbitrary shader-local/threadgroup pointers or a memory-safe runtime: GPU out-of-bounds access and use-after-free remain application errors.

**Acceptance:** Compare compiled host/shader sizes and offsets, then execute array/pointer arithmetic, nested reads/writes, and vertex pulling on every claiming backend. A compiler accepting a 64-bit integer is not proof of shader pointer support.

### REND-BIND-001 — Application-owned bindless texture/sampler heaps

**Required behavior:** The application chooses descriptor-heap capacities and slots. Rend reports storage/alignment constraints and writes texture-view/sampler descriptors at requested indices. Heaps may be opaque; writable native descriptor bytes are not a portable requirement. Materials carry indices; changing materials does not construct/rebind per-material sets.

Heap activation is command-context state. Support sampled/writable views where formats allow, with explicit sampler state. Define index width/ranges, nonuniform indexing, nulls, and update visibility in the shader ABI. Slot/reference lifetime follows [REND-SYNC-002](#rend-sync-002--explicit-completion-visibility-and-safe-reuse); native descriptor bytes/handles are device/backend-local.

**Acceptance:** Vary materials, samplers, sampled/storage views, and nonuniform indices through active heaps without per-material binding changes. Verify safe slot reuse and explicit rejection of unsupported devices.

### REND-BIND-002 — Root arguments instead of public binding tables

**Required behavior:** Each draw/dispatch receives one GPU address to an application-owned argument structure containing constants, GPU pointers, and texture/sampler indices. CPU/shaders share its declaration. No application descriptor-set layouts, pipeline layouts, root signatures, or binding tables are required; native binding machinery may remain internal.

One root address is shared by the active stages of each graphics command and supplied independently for compute. Recording captures the address, **not a snapshot of the root contents**. Commands requiring different values need distinct stable records or explicitly ordered GPU updates. Roots and reachable data/descriptors/resources follow [REND-SYNC-002](#rend-sync-002--explicit-completion-visibility-and-safe-reuse).

A conventional multi-draw command keeps one bound root. Its vertex shader receives a zero-based draw ID, reset for each multi-draw command; direct/single draws use zero. The application can index an array of records or GPU pointers reachable from that root. Instance IDs/base-instance values remain separate, and shaders pass any needed draw identity to fragment stages explicitly. No per-draw root rebind, per-material binding table, or GPU-selected pipeline framework is required.

**Acceptance:** Record multiple commands with different roots in one submission without pipeline/layout rebuilding on conventional graphics, compute, and task/mesh paths. Include nested pointers, texture indices, and CPU-/GPU-written roots with explicit producer/consumer ordering; reject incompatible shader ABIs.

## Textures and transfers

### REND-TEX-001 — Texture placement, views, and storage requirements

**Required behavior:** Query size/alignment for a texture description and an explicitly selected texture-storage class, then create over an application-selected compatible heap range. A successful query applies only to that description, device/class, and reported placement restrictions; creation revalidates them and does not substitute backing. Describe dimensions, format, mips, layers, samples, and intended operations. Query format/usage/sample capabilities; support 1D/2D/3D and cube views where available.

Storage, texture objects, views, and descriptors have distinct lifetimes. Report view compatibility, placement/aliasing restrictions, and required ordering. The application decides when to reclaim or alias compatible ranges under those rules. After safe native-object destruction, Rend must not indefinitely retain their placement. Opaque tiled storage is not a CPU pixel array.

**Acceptance:** Place sampled/storage/attachment textures/views, exercise supported mip/layer/format/sample combinations, and reclaim completed ranges. Reject undersized/incompatible placements without partial publication; test alias handoffs under the reported restrictions.

### REND-TEX-002 — Upload, copy, write, and readback

**Required behavior:** Record data-range copies, data-to-texture uploads, texture-to-data readback, and texture copies. Support bounded subregions, mips/layers, and format/block-aware row/slice pitches. Report staging size and transfer alignment; applications supply and retire staging ranges under [REND-SYNC-002](#rend-sync-002--explicit-completion-visibility-and-safe-reuse), not an implicit upload allocator.

Support transfer writes and storage-texture shader writes where available. Define overlap restrictions. Exact copies and blit/filter operations are distinct; presentation/scaling blits report supported formats, filters, and regions, not arbitrary format conversion. Upload-queue/convenience policy belongs in [optional utilities](#optional-utilities-and-exclusions).

**Acceptance:** Upload, copy/edit subregions/mips, compute-write, and read back after completion; compare exact pixels for exact operations. Check compressed pitches where supported, overlaps, bounds, incompatible formats, and repeated staging reuse without hidden backing.

## Commands and pipeline state

### REND-WORK-001 — Graphics, compute, and mesh work

**Required behavior:** Record graphics draws, compute dispatches, and task/mesh work, directly and through indirect argument ranges. Report native limits/alignment. Indexed/instanced graphics uses index-address ranges and vertex pulling, not public vertex buffers/layouts. Conventional non-indexed/indexed multi-draw with a GPU-written count is required in both profiles; task/mesh is required only in the full profile. An explicit graphics/compute profile is not a fallback.

#### Recording ownership and cost

- One caller at a time accesses a command pool and its list slots, including begin/end/reset. Independent pools in the same context can record concurrently using immutable resource/pipeline records and their private workspace. A pool per recording worker per in-flight batch is a caller policy, not a Rend frame manager.
- Each native queue is independently caller-serialized for submission/presentation. Queue-local scratch and submission slots cannot serialize unrelated queues or recorders. Management calls sharing context record allocation or management scratch are caller-serialized; destroying/mutating an object requires exclusion from its users, not a global lock on unrelated recording. Descriptor writes are serialized per descriptor heap; safe slot update still requires GPU lifetime/visibility discipline. Concurrent creation while workers read existing records must not relocate those records or require recording to use the management scratch.
- Record directly into native command storage. Ordinary recording/submission must not initiate pipeline compilation, heap/resource creation, resource-table scans, or implicit waits. Command begin/reset and native driver command-storage behavior are accounted separately. If a backend genuinely needs retained lowering metadata, document and bound it in its private pool partition; do not introduce a general replay/interpreter stream without demonstrated native need and cost evidence.
- Validate capabilities/configuration at discovery/creation and cheap state/range/capacity preconditions at use. Do not re-reflect shaders, rediscover devices, or infer accesses by walking root/descriptor graphs per draw. Native driver/compiler internal work is not a zero-cost guarantee; measure it separately where observable.

#### Recording, submission, and failure states

A list slot follows `initial → recording → executable → pending → completed`; whole-pool reset returns non-pending slots to `initial` and invalidates previous list handles. Lists are one-shot: after submission, rerecord only after completion and reset. No simultaneous-use or reusable command replay mode is required.

`begin` requires an initial slot and starts with no rendering operation or bound pipeline/heaps/dynamic state. State belongs to the list, never a previous recording or another list. Draws require compatible prepared pipeline/state and an active rendering operation; dispatch/transfer require no rendering operation. Rendering operations do not nest. Barrier legality follows the documented native pass/queue restrictions; Rend rejects an illegal placement rather than silently splitting a pass. `end` requires recording with no open rendering operation.

A command/end failure on a recording list invalidates it; it cannot be submitted or continued, and reset is the recovery path. A failed begin or a state-invalid call on a non-recording list leaves its existing state unchanged, especially when pending. Reset can discard unsubmitted recording/executable/invalid lists, but returns zero without resetting anything if any list in the pool remains pending. It does not wait. Submission's host-side access to lists is also serialized with their owning pools; this does not hold a pool lock during GPU execution. Known state errors are distinct from dangling-handle caller errors.

A submission targets one queue and one bounded batch. Preflight validates nonempty command count, configured limits, distinct executable lists from compatible queue families, context membership, wait stages, and the completion signal. Arrays are consumed during the call. Preflight failure submits nothing and leaves executable lists available; success marks every submitted list pending and copies the completion point into bounded queue bookkeeping. `completion` is a required queue-produced timeline signal covering all submitted work, irrespective of wait-stage scopes. Do not split a batch into partially successful public submissions or advance completion bookkeeping before native success.

A native submission failure makes the context terminal for new GPU work, even if the error is not reported as device loss: do not assume rollback, resubmit possibly accepted commands, or reuse their storage. Return zero and retain backing until safe teardown; the deferred diagnostic/state mechanism must expose the terminal condition and native/device-loss reason. Device loss in other operations likewise terminates new work. A wait timeout alone is not terminal and authorizes no reuse.

After stopping/joining context users, the caller can explicitly call `rend_backend_wait_idle` to drain GPU work, including a failed submission whose completion signal is uncertain. It is a blocking native idle operation with no portable timeout promise, not a routine frame operation or a hidden destroy step. Success proves GPU work is no longer pending, but does not make a terminal context reusable. Observed device loss permits the backend's documented native lost-device cleanup without waiting for unreachable timeline values. Any other drain failure leaves completion unproven: retain potentially used backing and report the failure rather than treating it as safe reclamation. Native presentation retirement remains a separate obligation. Teardown invalidates all remaining borrowed handles; no further work is accepted on a terminal context.

#### Indirect argument and root convention

These are shader-data records, not host handles. Their field order, widths, and sizes (16, 20, and 12 bytes respectively) are part of the verified shader ABI; no host pointers or compiler packing switches are involved.

```c
typedef struct RendDrawIndirectArgs {
	uint32_t vertex_count;
	uint32_t instance_count;
	uint32_t first_vertex;
	uint32_t first_instance;
} RendDrawIndirectArgs;

typedef struct RendDrawIndexedIndirectArgs {
	uint32_t index_count;
	uint32_t instance_count;
	uint32_t first_index;
	int32_t vertex_offset;
	uint32_t first_instance;
} RendDrawIndexedIndirectArgs;

typedef struct RendDispatchIndirectArgs {
	uint32_t x, y, z;
} RendDispatchIndirectArgs;

typedef struct RendIndirectDraws {
	RendHeapRange arguments;
	uint32_t stride;
	uint32_t max_draw_count;
	RendHeapRange count;               /* NULL heap and zero offset/size: use max_draw_count. */
} RendIndirectDraws;
```

For a supplied count range, consume one GPU-written `uint32_t` and execute `min(count, max_draw_count)` draws. Zero maximum/count executes no draws. For nonzero maximum, argument/count ranges belong to data heaps, stride is at least the selected record size and a multiple of four, and a supplied count range covers at least four bytes; report any stricter native address/stride limits. Validate `(max_draw_count - 1) * stride + record_size` against the argument extent with checked arithmetic. For zero maximum, no argument/count read occurs; both ranges and stride may be zero, but the command still requires valid recording/rendering state. GPU-written fields must meet native draw/dispatch limits; Rend does not read them back to validate them.

Use one root pointing to an application-owned draw table as specified in [REND-BIND-002](#rend-bind-002--root-arguments-instead-of-public-binding-tables). A producer dispatch can write the table, argument records, and count; explicit barriers make them visible to the vertex and indirect consumers. GPU-selected root *contents* and draw-table entries do not imply GPU-selected binding state. Single indirect dispatch is sufficient; arbitrary GPU command generation, pipeline selection, and multi-dispatch-count execution are not requirements. Query task/mesh indirect-record and multi-draw capabilities separately rather than pretending conventional records apply unchanged.

**Acceptance:** Compare direct/indirect graphics, compute, and full-profile task/mesh output. Test compute-generated conventional indexed/non-indexed arguments, distinct draw-table records, and counts of zero, below the maximum, and above it without CPU readback or per-draw root rebinding. Check strides/overflow/limits, state and capacity errors, failed-recording discard, submission preflight atomicity, terminal native failure, and whole-pool reuse after completion. Record equivalent work concurrently through independent pools and compare output with the serial path; measure CPU time and private retained storage under [REND-PERF-001](#rend-perf-001--representative-cost-and-performance-acceptance).

### REND-PIPE-001 — Narrow pipeline objects and independent state

**Required behavior:** Create graphics/compute/task-mesh pipelines from prepared native shader artifacts. Slang supplies shared source/ABI tooling; no runtime compiler is required. Pipelines retain rasterization, blending, and attachment formats, but no application resource-binding or vertex-layout declarations. Set viewport, scissor, and depth/stencil state independently.

Report unsupported shaders/states and native compilation failures. Rend may initiate native pipeline compilation/variant creation only during explicit pipeline creation or preparation, never during a setter, draw, dispatch, or submission. If native fixed state needs variants, the application declares the required finite state combinations at that preparation boundary; Rend reports their capacity/creation cost and prepares them before use. A setter selects native dynamic state or an already prepared variant; an unprepared combination fails without compilation or silent substitution. The application still need not construct separate native pipeline permutations. Unresolved lowering is a limitation, not conformance.

**Acceptance:** Vary viewport/scissor/depth-stencil without application pipeline permutations and reuse pipelines with different roots/materials. Check exhaustion, unprepared-state rejection, and creation-failure cleanup. Instrument native pipeline-creation calls to verify none originate in recording/submission. Measure preparation, first use, steady-state use, and native variant storage separately; bounded storage alone is not evidence of acceptable latency.

### REND-PASS-001 — Attachments and rendering operations

**Required behavior:** Begin/end rendering with explicit color/depth-stencil attachments, subresources, extent, load/store/clear operations, and supported resolves. Offscreen and acquired presentation targets share the work/data model. Load/preserve retains intended pixels within their lifetime, never an undefined target from a failed acquire.

**Acceptance:** Compare clear, preserve, store/discard, depth/stencil, and supported multisample results. Check bounds/formats and transfer→render→sample sequences.

## Synchronization and lifetime

### REND-SYNC-001 — Producer/consumer barriers without resource lists

**Required behavior:** Barriers describe producer/consumer stages and read/write visibility, without buffer/image lists or application image-layout tracking. Distinguish transfer, compute, index input, vertex, task, mesh, fragment, early/late depth-stencil tests, color output, indirect consumption, and host visibility. `GRAPHICS` is an aggregate of enabled graphics work, not the mandatory lowering for every graphics dependency; explicit unsupported stages fail. Backends must not broaden a precise dependency merely to simplify Rend bookkeeping; document broader native granularity where unavoidable.

For render-target sampling, permit `COLOR_OUTPUT/WRITE → FRAGMENT/READ` without making subsequent vertex work wait on the attachment producer. Access masks of `NONE` on both sides specify an execution-only dependency, useful for write-after-read hazards without an unnecessary memory flush. `NONE` stages have no execution scope; nonzero access requires a compatible nonempty stage scope. Reject unknown bits and impossible stage/access combinations. Host stages concern visibility only, never permission to overwrite pending GPU data.

The backend supplies a correct native state/layout strategy. Any bookkeeping is capacity-bounded and subresource-correct. Counting resources alone is not a strategy: bindless indices/pointer graphs do not reveal shader accesses to the recorder. Demonstrate how arbitrary permitted shader accesses, initial image use, presentation transitions, aliasing, and queue-family ownership remain valid without public resource lists or per-command whole-resource scans. Initialization/alias activation and presentation may use resource-specific native operations; ordinary hazards remain resource-free.

These are global work/memory dependencies, **not automatic hazard detection**. Applications specify hazards and order. Queue-local barriers do not synchronize other queues, wait on the CPU, or alone authorize reuse/aliasing. Global barriers and a `GENERAL` image strategy simplify the contract; they are not a universal performance guarantee. Even unified-layout hardware can benefit from resource-specific native barriers in some workloads. Keep the public model and measure/document that trade-off on target devices rather than silently adding a tracker or claiming optimality everywhere.

**Acceptance:** Validate transfer→shader, compute→graphics/indirect, render→sample/readback, execution-only write-after-read, write-after-write, and ordered aliasing on each backend without public resource lists/layouts. Include subresources and cross-queue native-state correctness. Compare precise color-output→fragment barriers with aggregate graphics barriers in a representative two-pass workload; measure GPU execution/overlap rather than inferring improvement from fewer flags.

### REND-SYNC-002 — Explicit completion, visibility, and safe reuse

**Required behavior:** Provide timeline-style submission waits/signals, completion queries, and CPU waits. Each timeline has one producing queue, chosen at creation; submissions on that queue signal strictly increasing values above its initial/previously submitted value, without wrap. Other queues in the same context may wait on it. A submission's completion signal covers all its work and earlier work on that producer queue; it does not complete independent queues. A larger completed value covers earlier values on that timeline. Values and arrays are copied, but referenced timeline objects stay live until every submitted signal/wait and retained list completion reference is retired/reset.

CPU query reports the observed completed value. CPU wait uses nanoseconds: zero polls, `UINT64_MAX` permits an indefinite wait, and other values request a finite native timeout; scheduling/native timer granularity can delay return, so this is not a real-time deadline. Expiry without completion and observed device loss both return zero rather than success; the deferred diagnostic/state mechanism must distinguish timeout from terminal device loss. No host signaling/reset or multi-producer timeline mode is required. Submission preflight checks queue ownership/value monotonicity, not arbitrary cross-queue deadlock cycles. The caller must arrange an acyclic, eventually signaled dependency chain, including waits for future submissions. Cross-queue and host↔GPU visibility remain explicit. No routine operation inserts an undocumented device-wide idle wait.

The application retains resources referenced by recorded commands until those commands are discarded or their submitted uses complete. While pending work can reference them, do not overwrite mapped data/descriptors, destroy objects, or release heap ranges. Ordered GPU writes remain permitted with the required dependencies. Direct CPU writes need the same visibility and lifetime discipline as uploads.

Completion of a command slot or acquisition of a presentation image protects only its documented scope, not all application allocations. Rend owns no implicit application frame allocator or deferred-deletion queue.

**Acceptance:** Submit multiple frames without intervening readback, varying geometry, roots, and material indices; verify each output after completion. Check safe ring wrap, cross-queue dependencies, timeouts/device loss, and teardown. Verify that mapped geometry is not overwritten while pending GPU work can read it; readback-synchronized frames alone do not demonstrate safe reuse.

## Backends and presentation

### REND-BACK-001 — Explicit capabilities and target backends

**Required behavior:** Target Vulkan 1.4, DirectX 12, and Metal 4 as separate build choices. Statically link exactly one `rend_backend_*.c` implementation at compile time; there is no public backend selector, automatic backend fallback, runtime DLL loading, hot reload, or required public vtable. Backend identity remains queryable for diagnostics and shader compatibility. Query every capability required by the explicitly selected device/profile/shader ABI, not just API versions. Reject missing requirements before publishing a context; report backend/device identity and the missing feature. Resources, addresses, and descriptor representations remain device/backend-local.

All targets must satisfy the same typed 64-bit GPU-pointer data ABI. DirectX 12 remains a required but unverified target: context creation must fail until an implementation satisfies that ABI. A descriptor-index/offset substitute does not satisfy the requirement. Static linking and declarations alone do not establish backend conformance.

| Backend    | Feasibility constraints and reference mappings; not Rend support                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Vulkan 1.4 | Buffer device addresses alone do not provide descriptor heaps or address-based native commands. Validate `VK_EXT_descriptor_heap`, `VK_KHR_device_address_commands`, shader-pointer/layout features, and task/mesh/dynamic state. These extensions are not guaranteed by core 1.4. Ordinary images can use `GENERAL`; enabled `VK_KHR_unified_image_layouts` guarantees layout efficiency where applicable, not that global barriers are optimal for every workload. Without it, measure/document layout costs. Initial/presentation/alias transitions still need handling. See the [reference Vulkan mapping](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/vulkan-support.md). |
| DirectX 12 | SM 6.6 plus Resource Binding Tier 3 provides direct texture/sampler heap indexing, **not arbitrary typed 64-bit GPU-pointer traversal**. Native root signatures and fixed depth/stencil pipeline state remain. A compiler/backend strategy must satisfy the specified address/data ABI; a descriptor-index/offset ABI is not automatically equivalent.                                                                                                                                                          |
| Metal 4    | The [reference Metal mapping](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/metal-support.md) documents GPU-root addresses, texture-view-pool indices, and sampler resource-ID arrays. Validate the shared Slang/layout convention, residency, independent state, and direct/indirect task/mesh limits on the selected devices. Application-selected slots do not imply writable native descriptor bytes or equality between slot indices and resource IDs.                                                                                                 |

**Acceptance:** Build each backend as a separate single-backend configuration and verify its reported identity. Report feature/ABI workloads and missing-feature rejection per backend, OS, device, driver, and compiler. Real native build/test evidence is required; unresolved features prevent full-conformance claims.

### REND-PRESENT-001 — Presentation and offscreen operation

**Required behavior:** Explicitly create, acquire, render/copy to, and present targets, handling resize, out-of-date, and minimized states. The host supplies native surface/window integration and owns input/window policy. Offscreen operation requires neither window nor desktop.

Acquired images are borrowed presentation resources, not application-placed textures. Define their lifetimes, frame/queue capacities, and wait/signal behavior, including native presentation synchronization that cannot use a timeline directly. Failed acquire returns zero/null and no drawable target. The deferred diagnostic/state mechanism must distinguish out-of-date targets requiring explicit recreation, timeout/no available target, and minimization with no drawable extent; none authorize submitting work to an absent target. A successful acquire obliges the caller to present or explicitly release/retire that acquisition under the native contract. Resize safely retires old targets within declared replacement/backing bounds; presentation completion must cover native use beyond rendering completion.

**Acceptance:** Test offscreen and native presentation separately, including failed acquire, minimization, resize/recreation, and pending-work teardown. Record built, tested, and supported status separately per platform.

### REND-BACK-002 — Runtime-loadable backend ABI (retired)

**Retired by owner decision:** Runtime backend loading, module ABI compatibility, hot reload, and runtime backend replacement are no longer requirements. The replacement contract is the single statically linked backend in [REND-BACK-001](#rend-back-001--explicit-capabilities-and-target-backends). This ID is retained for historical references and must not be reused.

## Optional utilities and exclusions

### REND-UTIL-001 — Independent utility layer

**Required behavior:** The graphics API works without utilities. Optional NoGraphicsAPIUtility integration or compatible existing helpers may supply shared shader types, math, allocators, upload queues, and deferred deletion; none become hidden mandatory Rend policies.

NoGraphicsAPIUtility uses C++ facilities. Decide C99 interoperability, dependencies, license, and packaging before integration. Its name commits neither vendoring nor a new utility framework; reuse before building.

**Acceptance:** Run a minimal heap/root/texture/dispatch client without utilities. If integration is chosen, separately demonstrate it and document added storage, lifetime, and toolchain requirements.

Not required by this specification: a software renderer, legacy descriptor-set frontend, automatic material/resource management, a render graph, automatic shader compilation, ray tracing, sparse resources, video APIs, or transparent migration of native objects across backends. These are exclusions from the current target, not a ban on later explicitly scoped features.

## Performance acceptance

### REND-PERF-001 — Representative cost and performance acceptance

**Required behavior:** Aim for practical, predictable costs on declared target devices, not universal optimality. Keep correctness/capability conformance separate from performance evidence. Define the tested OS/device/driver/compiler, build settings, workload size, and practical latency/memory budgets before judging a result. A supported path can have a documented performance limitation; do not invent an emulation framework or indefinitely redesign the API to chase an unmeasured optimum.

Compare Rend with equivalent direct-native recording/submission using the same shader artifacts, data placement, workload, and synchronization semantics. Use optimized builds without validation for timing; run validation-enabled correctness checks separately. Separate preparation, first use, and warmed steady state, with repeated samples reporting median and tail CPU recording/submission latency (p95/p99), GPU elapsed time, and relevant overlap. Host time blocked on a wait is not GPU elapsed time. Use native timestamps/profilers; a public profiling framework is not required.

| Representative workload | Evidence sought |
| --- | --- |
| UI/sprite draws with changing roots, clips, and textures | Small-command CPU cost and latency without per-material rebinding |
| Image-processing passes and render-target→sampling | GPU time with precise stages; cost of global barriers/`GENERAL` versus a valid native specialized path |
| Many draws with serial/parallel recording and GPU-counted multi-draw | CPU scaling, draw-table correctness, submission overhead, private pool memory |
| Upload/readback and asynchronous range/descriptor reuse | Transfer throughput, explicit waits, noncoherent behavior where available, safe wrap without churn |

Report peak live and retained Rend host bytes, GPU/native backing, allocation attempts, and exclusions separately. Do not count arena suballocation as new heap backing, native allocations as Rend allocations, fixed capacity as measured use, or a debug feasibility run as an application benchmark. Varying a native layout/barrier strategy is a separate experiment from measuring the Rend wrapper overhead. Cover UMA and discrete hardware before generalizing across them; unavailable hardware is an explicit evidence gap.

**Acceptance:** Retain exact-output/asynchronous/failure checks alongside the performance workloads. Compare results with the agreed practical budgets and equivalent native baseline; investigate material unexplained overhead or record the accepted limitation. No fixed percentage win, performance parity on every GPU, new benchmark framework, or optimization of noise is required for completion. Reuse existing test assets. Validation reports must distinguish measured results from target budgets; this specification defines requirements, not measured outcomes.

## Technical references

Design/toolchain references explain possible mappings, not Rend conformance. Upstream support reports are not substitutes for native Rend tests.

- [No Graphics API essay](https://www.sebastianaaltonen.com/blog/no-graphics-api), [NoGraphicsAPI / utilities](https://github.com/sebbbi/NoGraphicsAPI), and its [shared Slang/root ABI](https://github.com/sebbbi/NoGraphicsAPI/blob/main/docs/slang.md).
- Vulkan [descriptor heaps](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_descriptor_heap.html), [address commands](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_address_commands.html), [untyped pointers](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_shader_untyped_pointers.html), and [unified image layouts](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_unified_image_layouts.html).
- Vulkan [precise pipeline barriers](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html), [unified-layout efficiency and remaining barrier trade-offs](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_unified_image_layouts.html), and [resource memory-type compatibility](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryRequirements.html).
- Slang [target compatibility](https://docs.shader-slang.org/en/stable/external/slang/docs/target-compatibility.html), [language/pointers](https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/02-conventional-features.html), and [Metal lowering](https://docs.shader-slang.org/en/stable/external/slang/docs/user-guide/a2-02-metal-target-specific.html).
- [DirectX SM 6.6 dynamic resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html).
- Apple [Metal 4 core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api), [texture view pools](https://developer.apple.com/documentation/metal/mtltextureviewpool), and [feature tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf).
