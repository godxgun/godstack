/* Rend 2.0.0 feasibility workload, not the public Rend shader ABI.
 * C99 hosts serialize GPU addresses as uint64_t; Slang dereferences typed pointers.
 * All data uses C layout and row-major matrices. No host pointer is serialized.
 */
#ifndef REND_ABI_SHARED_H
#define REND_ABI_SHARED_H

#ifdef __SLANG__
#define ABI_POINTER(type) type*
#define ABI_FLOAT2 float2
#define ABI_FLOAT3 float3
#define ABI_MATRIX float3x2
#else
#include <stdint.h>
#define ABI_POINTER(type) uint64_t
#define ABI_FLOAT2 AbiFloat2
#define ABI_FLOAT3 AbiFloat3
#define ABI_MATRIX AbiMatrix
typedef struct AbiFloat2 { float x, y; } AbiFloat2;
typedef struct AbiFloat3 { float x, y, z; } AbiFloat3;
typedef struct AbiMatrix { float rows[3][2]; } AbiMatrix;
#endif

#define ABI_WIDTH 16
#define ABI_HEIGHT 16
#define ABI_FRAMES 3
#define ABI_ELEMENTS 16
#define ABI_LAYOUT_WORDS 18

struct AbiNode {
	ABI_POINTER(uint32_t) values;
	ABI_POINTER(AbiNode) next;
	uint32_t bias;
	uint32_t enabled;
};

struct AbiVertex {
	ABI_FLOAT2 position;
	float depth;
};

struct AbiRoot {
	uint32_t tag;
	ABI_FLOAT3 vector;
	ABI_MATRIX matrix;
	ABI_POINTER(AbiNode) node;
	ABI_POINTER(AbiVertex) vertices;
	ABI_POINTER(uint32_t) output;
	ABI_POINTER(AbiRoot) generated;
	ABI_POINTER(AbiRoot) probe;
	uint32_t texture_first;
	uint32_t sampler_first;
	uint32_t storage_slot;
	uint32_t count;
	uint32_t draw_slot;
	uint32_t reserved;
};

#ifndef __SLANG__
typedef struct AbiNode AbiNode;
typedef struct AbiVertex AbiVertex;
typedef struct AbiRoot AbiRoot;
#endif

#undef ABI_POINTER
#undef ABI_FLOAT2
#undef ABI_FLOAT3
#undef ABI_MATRIX
#endif
