// Native Vulkan feasibility only. No descriptor sets or vertex-input layouts.
#include "rend_abi_shared.h"

[[vk::binding(0, 0)]] ConstantBuffer<AbiRoot> root;

[shader("compute")]
[numthreads(16, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
	uint i = tid.x;
	if (i >= root.count)
		return;
	uint slot = NonUniformResourceIndex(root.texture_first + (i & 1));
	uint samplerSlot = NonUniformResourceIndex(root.sampler_first + (i & 1));
	Texture2D<float4> texture = getDescriptorFromHandle(DescriptorHandle<Texture2D<float4>>(uint2(slot, 0)));
	SamplerState sampler = getDescriptorFromHandle(DescriptorHandle<SamplerState>(uint2(samplerSlot, 0)));
	WTexture2D<float4> storage = getDescriptorFromHandle(DescriptorHandle<WTexture2D<float4>>(uint2(root.storage_slot, 0)));
	float4 color = texture.SampleLevel(sampler, float2(0.5, 0.5), 0);
	storage.Store(int2(i, 0), color);
	root.output[i] = root.tag + root.node->bias + root.node->next->values[i] + uint(color.r * 255.0 + 0.5);
	if (i == 0) {
		AbiRoot result = root;
		result.tag += 7;
		*root.generated = result;
		uint32_t* layout = root.output + ABI_ELEMENTS;
		layout[0] = sizeof(AbiRoot);
		layout[1] = sizeof(AbiNode);
		layout[2] = sizeof(AbiVertex);
		layout[3] = sizeof(uint64_t);
		layout[4] = uint(uint64_t(&root.probe->tag) - uint64_t(root.probe));
		layout[5] = uint(uint64_t(&root.probe->vector) - uint64_t(root.probe));
		layout[6] = uint(uint64_t(&root.probe->matrix) - uint64_t(root.probe));
		layout[7] = uint(uint64_t(&root.probe->node) - uint64_t(root.probe));
		layout[8] = uint(uint64_t(&root.probe->vertices) - uint64_t(root.probe));
		layout[9] = uint(uint64_t(&root.probe->output) - uint64_t(root.probe));
		layout[10] = uint(uint64_t(&root.probe->generated) - uint64_t(root.probe));
		layout[11] = uint(uint64_t(&root.probe->probe) - uint64_t(root.probe));
		layout[12] = uint(uint64_t(&root.probe->texture_first) - uint64_t(root.probe));
		layout[13] = uint(uint64_t(&root.probe->count) - uint64_t(root.probe));
		layout[14] = uint(root.vector.x + root.vector.y * 10 + root.vector.z * 100);
		layout[15] = uint(root.matrix[0][0] + root.matrix[1][0] * 10 + root.matrix[2][1] * 100);
		layout[16] = root.node->enabled;
		layout[17] = root.node->next->next == nullptr ? 1 : 0;
	}
}

[shader("compute")]
[numthreads(16, 1, 1)]
void consumeMain(uint3 tid : SV_DispatchThreadID)
{
	uint i = tid.x;
	if (i < root.count)
		root.output[i] += root.tag;
}

struct VertexOutput {
	float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint id : SV_VertexID)
{
	AbiVertex v = root.vertices[id];
	VertexOutput result;
	result.position = float4(v.position, v.depth, 1);
	return result;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
	Texture2D<float4> texture = getDescriptorFromHandle(DescriptorHandle<Texture2D<float4>>(uint2(root.draw_slot, 0)));
	SamplerState sampler = getDescriptorFromHandle(DescriptorHandle<SamplerState>(uint2(root.sampler_first, 0)));
	return texture.SampleLevel(sampler, float2(0.5, 0.5), 0);
}
