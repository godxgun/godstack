struct VertexInput {
    [[vk::location(0)]] float2 pos : POSITION;
    [[vk::location(1)]] float4 col : COLOR;
};

struct VertexOutput {
    float4 col : COLOR;
    float4 pos : SV_Position;
};

struct UBO {
    float2 viewport;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UBO> ubo;

[shader("vertex")]
VertexOutput vertMain(VertexInput input)
{
    VertexOutput output;
    output.col = input.col;
    output.pos = float4(input.pos*2.0/ubo.viewport - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target
{
    return input.col;
}
