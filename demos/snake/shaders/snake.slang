struct VSInput {
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float inShade : TEXCOORD0;
    [[vk::location(2)]] float3 instancePos : TEXCOORD1;
    [[vk::location(3)]] float3 instanceScale : TEXCOORD2;
    [[vk::location(4)]] float3 instanceColor : TEXCOORD3;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
};

struct PushConstants {
    float4x4 mvp;
};

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float3 world = input.inPosition * input.instanceScale + input.instancePos;
    output.pos = mul(pc.mvp, float4(world, 1.0));
    output.color = input.instanceColor * (0.35 + 0.65 * input.inShade);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
