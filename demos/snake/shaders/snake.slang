// Shared indexed cube geometry and instance stream; native Vulkan shader artifact.
struct VSInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float shade : TEXCOORD0;
    [[vk::location(2)]] float3 offset : TEXCOORD1;
    [[vk::location(3)]] float3 scale : TEXCOORD2;
    [[vk::location(4)]] float3 color : COLOR0;
};
struct VSOutput { float4 pos : SV_Position; float3 color : COLOR0; };
struct PushConstants { float4x4 transform; };
[[vk::push_constant]] PushConstants pc;
[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos=mul(pc.transform,float4(input.position*input.scale+input.offset,1.0));
    output.color=input.color*(0.35+0.65*input.shade);
    return output;
}
[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target { return float4(input.color,1.0); }
