struct VSInput {
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(1)]] float4 col : COLOR0;
};

struct VSOutput {
    float4 pos : SV_Position;
    float4 col : COLOR0;
};

struct PushConstants {
    float4 screen;
};

[[vk::push_constant]]
PushConstants pc;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float2 ndc;
    ndc.x = input.pos.x / pc.screen.x * 2.0 - 1.0;
    ndc.y = input.pos.y / pc.screen.y * 2.0 - 1.0;
    output.pos = float4(ndc, 0.0, 1.0);
    output.col = input.col;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    return input.col;
}
