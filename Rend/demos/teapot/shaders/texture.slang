struct VSInput {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float2 offset;
};

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv;
};

[[vk::binding(0, 0)]] ConstantBuffer<float4x4> ubos[3];
[[vk::binding(1, 0)]] Sampler2D test[3];

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.pos.x -= input.offset.x;
    output.pos.y -= input.offset.y;
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return test[0].Sample(vertIn.uv);
}
