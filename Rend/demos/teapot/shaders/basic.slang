struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
};

struct PushConstants {
    float4x4 ubo;
};

[[vk::binding(0, 0)]] ConstantBuffer<float4x4> ubos[3];
[[vk::binding(1, 0)]] Sampler2D test[3];

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    
    output.pos = mul(pc.ubo, float4(input.inPosition, 1.0));
    output.color = input.inPosition;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    float4 col = test[0].Sample(vertIn.uv);
    return float4(col.rgb, 1.0); 
}
