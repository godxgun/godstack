struct VSInput {
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float4 col : COLOR0;
};

struct VSOutput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

struct PushConstants {
    float4 screen; /* xy framebuffer px, zw atlas px */
};

[[vk::push_constant]]
PushConstants pc;

[[vk::binding(0, 0)]]
Sampler2D atlas[1];

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float2 ndc;
    ndc.x = input.pos.x / pc.screen.x * 2.0 - 1.0;
    ndc.y = input.pos.y / pc.screen.y * 2.0 - 1.0;
    output.pos = float4(ndc, 0.0, 1.0);
    output.uv = input.uv;
    output.col = input.col;
    return output;
}

/* Coverage is linear. The swapchain is UNORM viewed as sRGB, and the
 * sampler is linear. Snap to a texel center so the filter does not mix
 * neighbors, then encode coverage so a 1:1 quad stays crisp. */
[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    float2 extent = max(pc.screen.zw, float2(1.0, 1.0));
    float2 uv = (floor(input.uv * extent) + 0.5) / extent;
    float cov = atlas[0].Sample(uv).r;
    if (cov <= 0.0031308)
        cov = cov * 12.92;
    else
        cov = 1.055 * pow(cov, 1.0 / 2.4) - 0.055;
    return float4(input.col.rgb, input.col.a * saturate(cov));
}
