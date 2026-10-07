/* Snake GPU data ABI shared by C99 and Slang. Pointers are GPU addresses on C. */
#ifndef SNAKE_GPU_H
#define SNAKE_GPU_H

#ifdef __SLANG__
#define SNAKE_GPU_PTR(type) type *
#define SNAKE_GPU_FLOAT3 float3
#define SNAKE_GPU_MATRIX float4x4
#else
#include <stddef.h>
#include <stdint.h>
#define SNAKE_GPU_PTR(type) uint64_t
#define SNAKE_GPU_FLOAT3 SnakeGpuFloat3
#define SNAKE_GPU_MATRIX SnakeGpuMatrix
#define SNAKE_GPU_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]
typedef struct SnakeGpuFloat3 {
	float x, y, z;
} SnakeGpuFloat3;
typedef struct SnakeGpuMatrix {
	float rows[4][4];
} SnakeGpuMatrix;
#endif

#define SNAKE_GPU_VERTEX_COUNT 24u
#define SNAKE_GPU_INSTANCE_CAPACITY (16u * 16u + 16u * 16u + 8u)

/* Keep the first member aligned to a 16-byte boundary in the serialized root. */
struct SnakeGpuVertex {
	SNAKE_GPU_FLOAT3 pos;
	float shade;
};

struct SnakeGpuInstance {
	SNAKE_GPU_FLOAT3 pos;
	float pos_pad;
	SNAKE_GPU_FLOAT3 scale;
	float scale_pad;
	SNAKE_GPU_FLOAT3 color;
	float color_pad;
};

struct SnakeGpuRoot {
	SNAKE_GPU_MATRIX mvp;
	SNAKE_GPU_PTR(SnakeGpuVertex)
	vertices;
	SNAKE_GPU_PTR(SnakeGpuInstance)
	instances;
	uint32_t instance_count;
	uint32_t reserved;
};

#ifndef __SLANG__
typedef struct SnakeGpuVertex SnakeGpuVertex;
typedef struct SnakeGpuInstance SnakeGpuInstance;
typedef struct SnakeGpuRoot SnakeGpuRoot;

SNAKE_GPU_ASSERT(snake_gpu_assert_address_width, sizeof(uint64_t) == 8);
SNAKE_GPU_ASSERT(snake_gpu_assert_vertex_size, sizeof(SnakeGpuVertex) == 16);
SNAKE_GPU_ASSERT(snake_gpu_assert_vertex_position, offsetof(SnakeGpuVertex, pos) == 0);
SNAKE_GPU_ASSERT(snake_gpu_assert_vertex_shade, offsetof(SnakeGpuVertex, shade) == 12);
SNAKE_GPU_ASSERT(snake_gpu_assert_matrix_size, sizeof(SnakeGpuMatrix) == 64);
SNAKE_GPU_ASSERT(snake_gpu_assert_instance_size, sizeof(SnakeGpuInstance) == 48);
SNAKE_GPU_ASSERT(snake_gpu_assert_instance_position, offsetof(SnakeGpuInstance, pos) == 0);
SNAKE_GPU_ASSERT(snake_gpu_assert_instance_scale, offsetof(SnakeGpuInstance, scale) == 16);
SNAKE_GPU_ASSERT(snake_gpu_assert_instance_color, offsetof(SnakeGpuInstance, color) == 32);
SNAKE_GPU_ASSERT(snake_gpu_assert_root_matrix, offsetof(SnakeGpuRoot, mvp) == 0);
SNAKE_GPU_ASSERT(snake_gpu_assert_root_vertices, offsetof(SnakeGpuRoot, vertices) == 64);
SNAKE_GPU_ASSERT(snake_gpu_assert_root_instances, offsetof(SnakeGpuRoot, instances) == 72);
SNAKE_GPU_ASSERT(snake_gpu_assert_root_count, offsetof(SnakeGpuRoot, instance_count) == 80);
SNAKE_GPU_ASSERT(snake_gpu_assert_root_size, sizeof(SnakeGpuRoot) == 88);
#endif

#undef SNAKE_GPU_PTR
#undef SNAKE_GPU_FLOAT3
#undef SNAKE_GPU_MATRIX
#ifndef __SLANG__
#undef SNAKE_GPU_ASSERT
#endif
#endif
