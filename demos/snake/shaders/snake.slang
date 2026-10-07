// Vertex pulling snake renderer. Compile with -fvk-use-c-layout -matrix-layout-row-major
// -capability spvDescriptorHeapEXT. No vertex layouts or push-constant payload.
#include "../snake_gpu.h"

[[vk::binding(0, 0)]] ConstantBuffer<SnakeGpuRoot> root;

struct VSOutput {
	float4 pos : SV_Position;
	float3 color : COLOR0;
};

[shader("vertex")]
VSOutput vertMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	SnakeGpuVertex vertex = root.vertices[vertexID];
	SnakeGpuInstance instance = root.instances[instanceID];
	float3 world = vertex.pos * instance.scale + instance.pos;
	VSOutput output;
	output.pos = mul(root.mvp, float4(world, 1.0));
	output.color = instance.color * (0.35 + 0.65 * vertex.shade);
	return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET
{
	return float4(input.color, 1.0);
}
